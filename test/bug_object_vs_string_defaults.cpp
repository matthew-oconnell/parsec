"// Test case to investigate in detail the bug with parsec handling string defaults
// This test provides a more detailed exploration of when the bug occurs

#include <string>
#include <iostream>
#include <fstream>
#include <cassert>
#include <vector>
#include <map>

#include <ps/parsec.h>
#include <ps/validate.h>

            // Helper function to check if a value is a string with the expected value
            bool
            testStringField(const ps::Dictionary& dict,
                            const std::string& path,
                            const std::string& expected) {
    std::cout << \"Testing path '\" << path << \"' for expected value: '\" << expected << \"'\" << std::endl;

        try {
        // Split the path by dots to navigate the dictionary
        std::vector<std::string> parts;
        std::string part;
        for (char c : path) {
            if (c == '.') {
                parts.push_back(part);
                part.clear();
            } else {
                part += c;
            }
        }
        if (!part.empty()) {
            parts.push_back(part);
        }

        // Navigate to the specified path
        const ps::Dictionary* current = &dict;
        for (const auto& p : parts) {
            if (!current->has(p)) {
                std::cout << \"Field '\" << p << \"' not found in path\" << std::endl;
                            return false;
            }
            current = &(*current)[p];
        }

        // Check the type
        std::string actual_type = current->type();
        if (actual_type != \"string\") {
            std::cout << \"Field has wrong type: '\" << actual_type << \"' (expected 'string')\" << std::endl;
            std::cout << \"Field dump: \" << current->dump() << std::endl;
            return false;
    }

    // Check the value
    std::string actual_value = current->asString();
    if (actual_value != expected) {
        std::cout
            << \"Field has wrong value: '\" << actual_value << \"' (expected '\" << expected << \"')\" << std::endl;
            return false;
    }

    std::cout << \"✓ Path '\" << path << \"' has correct string value: '\" << actual_value << \"'\" << std::endl;
        return true;
}
catch (const std::exception& e) {
    std::cout << \"Error testing path '\" << path << \"': \" << e.what() << std::endl;
                return false;
}
}

// Function to create test cases with varying schema complexity
void runTestCase(const std::string& test_name,
                 const std::string& schema,
                 const std::string& input) {
    std::cout << \"\\n========== Running test case: \" << test_name << \" ==========\" << std::endl;

                try {
        ps::Dictionary schema_dict = ps::parse(schema);
        ps::Dictionary user_dict = ps::parse(input);

        // Save the input and schema to files for debugging
        std::ofstream schema_file(\"parsec_bug_\" + test_name + \"_schema.json\");
        schema_file << schema;
        schema_file.close();
        
        std::ofstream input_file(\"parsec_bug_\" + test_name + \"_input.json\");
        input_file << input;
        input_file.close();
        
        std::cout << \"1. Parsed schema and input successfully.\" << std::endl;
        
        // Apply defaults from schema to user input
        ps::Dictionary merged_dict = ps::setDefaults(user_dict, schema_dict);
        std::cout << \"2. Applied defaults successfully.\" << std::endl;
        
        // Save the resulting dictionary for debugging
        std::ofstream result_file(\"parsec_bug_\" + test_name + \"_result.json\");
        result_file << merged_dict.dump();
        result_file.close();
        
        // Now test the field with the expected defaults
        bool success = testStringField(merged_dict, \"settings.frechet derivative type\", \"finite-difference\");
        
        if (success) {
            std::cout << \"✓ Test case '\" << test_name << \"' PASSED\" << std::endl;
        } else {
            std::cout << \"✗ Test case '\" << test_name << \"' FAILED\" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << \"Error in test case '\" << test_name << \"': \" << e.what() << std::endl;
    }
}

int main() {
    // Test case 1: Simple schema with just the field with default
    {
        std::string schema = R\"({
            \"type\": \"object\",
            \"properties\": {
                \"settings\": {
                    \"type\": \"object\",
                    \"properties\": {
                        \"frechet derivative type\": {
                            \"type\": \"string\",
                            \"default\": \"finite-difference\",
                            \"enum\": [\"none\", \"finite-difference\", \"analytic-fd\", \"analytic-full\"]
                        }
                    }
    }
    }
    })\";
        
        std::string input = R\"({
            \"settings\": {}
    })\";
        
        runTestCase(\"simple\", schema, input);
    }

    // Test case 2: More complex schema with multiple fields
    {
        std::string schema = R\"({
            \"type\": \"object\",
            \"properties\": {
                \"settings\": {
                    \"type\": \"object\",
                    \"properties\": {
                        \"frechet derivative type\": {
                            \"type\": \"string\",
                            \"default\": \"finite-difference\",
                            \"enum\": [\"none\", \"finite-difference\", \"analytic-fd\", \"analytic-full\"]
                        },
                        \"solver\": {
                            \"type\": \"string\",
                            \"default\": \"jfnk\",
                            \"enum\": [\"newton\", \"jfnk\"]
    }
    }
    }
    }
    })\";
        
        std::string input = R\"({
            \"settings\": {}
    })\";
        
        runTestCase(\"multiple_fields\", schema, input);
    }

    // Test case 3: Explicit schema version
    {
        std::string schema = R\"({
            \"$schema\": \"http://json-schema.org/draft-07/schema#\",
            \"type\": \"object\",
            \"properties\": {
                \"settings\": {
                    \"type\": \"object\",
                    \"properties\": {
                        \"frechet derivative type\": {
                            \"type\": \"string\",
                            \"default\": \"finite-difference\",
                            \"enum\": [\"none\", \"finite-difference\", \"analytic-fd\", \"analytic-full\"]
                        }
                    }
    }
    }
    })\";
        
        std::string input = R\"({
            \"settings\": {}
    })\";
        
        runTestCase(\"explicit_schema_version\", schema, input);
    }

    // Test case 4: Deep nesting similar to your actual schema
    {
        std::string schema = R\"({
            \"type\": \"object\",
            \"properties\": {
                \"HyperSolve\": {
                    \"type\": \"object\",
                    \"properties\": {
                        \"nonlinear solver settings\": {
                            \"type\": \"object\",
                            \"properties\": {
                                \"frechet derivative type\": {
                                    \"type\": \"string\",
                                    \"default\": \"finite-difference\",
                                    \"enum\": [\"none\", \"finite-difference\", \"analytic-fd\", \"analytic-full\"]
                                }
                            }
    }
    }
    }
    }
    })\";
        
        std::string input = R\"({
            \"HyperSolve\": {
                \"nonlinear solver settings\": {}
    }
    })\";
        
        runTestCase(\"deep_nesting\", schema, input);
    }

    // Test case 5: Try with a direct string value to see if that works
    {
        std::string schema = R\"({
            \"type\": \"object\",
            \"properties\": {
                \"settings\": {
                    \"type\": \"object\",
                    \"properties\": {
                        \"frechet derivative type\": {
                            \"type\": \"string\",
                            \"enum\": [\"none\", \"finite-difference\", \"analytic-fd\", \"analytic-full\"]
                        }
                    }
    }
    }
    })\";
        
        std::string input = R\"({
            \"settings\": {
                \"frechet derivative type\": \"finite-difference\"
    }
    })\";
        
        runTestCase(\"direct_string_value\", schema, input);
    }

    // Test case 6: Special characters in field names
    {
        std::string schema = R\"({
            \"type\": \"object\",
            \"properties\": {
                \"settings\": {
                    \"type\": \"object\",
                    \"properties\": {
                        \"simple_name\": {
                            \"type\": \"string\",
                            \"default\": \"finite-difference\",
                            \"enum\": [\"none\", \"finite-difference\", \"analytic-fd\", \"analytic-full\"]
                        }
                    }
    }
    }
    })\";
        
        std::string input = R\"({
            \"settings\": {}
    })\";
        
        runTestCase(\"simple_name\", schema, input);
    }

    return 0;
    }
    "