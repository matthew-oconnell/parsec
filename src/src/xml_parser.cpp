#include <ps/xml.h>
#include <ps/dictionary.h>
#include <string>
#include <stdexcept>
#include <cctype>

namespace ps {

// Very small, forgiving XML parser sufficient for simple configs.
// - Ignores XML prolog (`<?xml ... ?>`).
// - Parses elements, attributes, nested elements and text nodes.
// - Attributes are stored under a child object named "@attributes".
// - Text content is stored as the scalar value when an element has only text,
//   otherwise under the key "#text".

namespace {
    const std::string whitespace = " \t\n\r";

    void skip_ws(const std::string& s, size_t& i) {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    }

    std::string parse_name(const std::string& s, size_t& i) {
        size_t start = i;
        while (i < s.size() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_' ||
                                s[i] == '-' || s[i] == ':'))
            ++i;
        if (i == start) throw std::runtime_error("XML parse error: expected name");
        return s.substr(start, i - start);
    }

    std::string parse_quoted(const std::string& s, size_t& i) {
        if (i >= s.size() || (s[i] != '"' && s[i] != '\''))
            throw std::runtime_error("XML parse error: expected quoted value");
        char q = s[i++];
        std::string out;
        while (i < s.size() && s[i] != q) {
            if (s[i] == '&') {
                // handle a couple of common entities
                if (s.compare(i, 5, "&amp;") == 0) {
                    out.push_back('&');
                    i += 5;
                } else if (s.compare(i, 4, "&lt;") == 0) {
                    out.push_back('<');
                    i += 4;
                } else if (s.compare(i, 4, "&gt;") == 0) {
                    out.push_back('>');
                    i += 4;
                } else if (s.compare(i, 6, "&quot;") == 0) {
                    out.push_back('\"');
                    i += 6;
                } else if (s.compare(i, 6, "&apos;") == 0) {
                    out.push_back('\'');
                    i += 6;
                } else {
                    out.push_back('&');
                    ++i;
                }
            } else {
                out.push_back(s[i++]);
            }
        }
        if (i >= s.size() || s[i] != q)
            throw std::runtime_error("XML parse error: unterminated attribute value");
        ++i;
        return out;
    }

    // Parse one element and return its Dictionary representation (without the element name)
    Dictionary parse_element(const std::string& s, size_t& i) {
        skip_ws(s, i);
        if (i >= s.size() || s[i] != '<') throw std::runtime_error("XML parse error: expected '<'");
        ++i;

        // Closing tag handled by caller
        if (i < s.size() && s[i] == '/')
            throw std::runtime_error("XML parse error: unexpected closing tag");

        std::string name = parse_name(s, i);
        Dictionary node;

        // parse attributes
        Dictionary attrs;
        while (true) {
            skip_ws(s, i);
            if (i >= s.size())
                throw std::runtime_error("XML parse error: unexpected EOF in start tag");
            if (s[i] == '/') {  // self-closing
                ++i;
                if (i >= s.size() || s[i] != '>')
                    throw std::runtime_error("XML parse error: expected '>' after '/'");
                ++i;
                if (!attrs.empty()) node["@attributes"] = attrs;
                return node;
            }
            if (s[i] == '>') {
                ++i;
                break;
            }
            // attribute
            std::string aname = parse_name(s, i);
            skip_ws(s, i);
            if (i >= s.size() || s[i] != '=')
                throw std::runtime_error("XML parse error: expected '=' after attribute name");
            ++i;
            skip_ws(s, i);
            std::string aval = parse_quoted(s, i);
            attrs[aname] = aval;
        }

        // parse children or text until closing tag
        std::map<std::string, std::vector<Dictionary>> children;
        std::string accumulated_text;

        while (true) {
            skip_ws(s, i);
            if (i >= s.size())
                throw std::runtime_error("XML parse error: unexpected EOF in content");
            if (s[i] == '<') {
                if (i + 1 < s.size() && s[i + 1] == '/') {
                    // closing tag
                    i += 2;
                    std::string endname = parse_name(s, i);
                    skip_ws(s, i);
                    if (i >= s.size() || s[i] != '>')
                        throw std::runtime_error("XML parse error: expected '>' at end tag");
                    ++i;
                    break;
                }
                if (i + 1 < s.size() && s[i + 1] == '?') {
                    // processing instruction, skip until '?>'
                    i += 2;
                    size_t pi_end = s.find("?>", i);
                    if (pi_end == std::string::npos)
                        throw std::runtime_error(
                                    "XML parse error: unterminated processing instruction");
                    i = pi_end + 2;
                    continue;
                }
                // child element
                Dictionary child = parse_element(s, i);
                // parse_element consumed the child's start tag and its contents, but we need the
                // child's name to get the name we look backward: find the last '<' before where
                // child parsing began is non-trivial. Instead, temporarily reparse the child by
                // scanning forward from the position where it started. Simpler approach: modify
                // parse_element to return pair(name, Dictionary) — but to keep this small, we will
                // search backward for the start of this child (not ideal but sufficient for simple
                // XML). Find the tag name by scanning backwards from current position to find the
                // most recent '<' followed by name.
                size_t back = i;
                // walk back to find the '<' of the child start
                while (back > 0) {
                    --back;
                    if (s[back] == '<' && back + 1 < s.size() && s[back + 1] != '/' &&
                        s[back + 1] != '?') {
                        // parse name at back+1
                        size_t tmp = back + 1;
                        try {
                            std::string child_name = parse_name(s, tmp);
                            // simple heuristic: accept this as child name
                            children[child_name].push_back(child);
                            break;
                        } catch (...) {
                            continue;
                        }
                    }
                }
                continue;
            }
            // text node
            size_t start = i;
            while (i < s.size() && s[i] != '<') ++i;
            accumulated_text += s.substr(start, i - start);
        }

        // attach attributes
        if (!attrs.empty()) node["@attributes"] = attrs;

        // attach children
        for (auto const& kv : children) {
            const auto& name = kv.first;
            const auto& vec = kv.second;
            if (vec.size() == 1)
                node[name] = vec[0];
            else
                node[name] = std::vector<Dictionary>(vec.begin(), vec.end());
        }

        // attach text
        // trim
        auto trim = [](const std::string& t) {
            size_t a = 0, b = t.size();
            while (a < b && std::isspace(static_cast<unsigned char>(t[a]))) ++a;
            while (b > a && std::isspace(static_cast<unsigned char>(t[b - 1]))) --b;
            return t.substr(a, b - a);
        };
        std::string text = trim(accumulated_text);
        if (!text.empty()) {
            if (node.size() == 0 && node.type() == Dictionary::Object && node.keys().empty() &&
                attrs.empty()) {
                // element with only text -> return as scalar
                Dictionary out = text;
                return out;
            }
            node["#text"] = text;
        }

        return node;
    }
}

Dictionary parse_xml(const std::string& text) {
    size_t i = 0;
    // skip BOM if present
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB &&
        (unsigned char)text[2] == 0xBF)
        i = 3;
    skip_ws(text, i);
    // skip optional XML prolog
    if (i + 1 < text.size() && text[i] == '<' && text[i + 1] == '?') {
        size_t end = text.find("?>", i + 2);
        if (end == std::string::npos)
            throw std::runtime_error("XML parse error: unterminated prolog");
        i = end + 2;
    }

    skip_ws(text, i);
    if (i >= text.size()) return Dictionary();

    // Expect a single root element — parse its name first
    if (text[i] != '<') throw std::runtime_error("XML parse error: expected root element");
    // parse root name to capture it
    size_t j = i + 1;
    std::string root_name = parse_name(text, j);

    // call parse_element which will consume the root and return its contents
    Dictionary root_contents = parse_element(text, i);

    Dictionary out;
    out[root_name] = root_contents;
    return out;
}

}  // namespace ps
