// Test case to demonstrate an issue with parsec's handling of default values
// This test shows a bug where a string value becomes an object when using setDefaults

#include <string>
#include <iostream>
#include <cassert>

#include <ps/parsec.h>
#include <ps/validate.h>

int main() {
    // Create a minimal schema where "frechet derivative type" should be a string with default value
    std::string schema = R"({
        "type": "object",
        "properties": {
            "settings": {
                "type": "object",
                "properties": {
                    "frechet derivative type": {
                        "type": "string",
                        "default": "finite-difference",
                        "enum": ["none", "finite-difference", "analytic-fd", "analytic-full"]
                    }
                }
            }
        }
    })";

    // Create a minimal user input with missing "frechet derivative type"
    std::string user_input = R"({
        "settings": {}
    })";

    // Parse the schema and user input
    ps::Dictionary schema_dict = ps::parse(schema);
    ps::Dictionary user_dict = ps::parse(user_input);

    // Apply defaults from schema to user input
    ps::Dictionary merged_dict = ps::setDefaults(user_dict, schema_dict);

    // Check that the frechet derivative type exists
    assert(merged_dict.has("settings"));
    assert(merged_dict["settings"].has("frechet derivative type"));

    // Get the type of the frechet derivative type field
    std::string frechet_type = merged_dict["settings"]["frechet derivative type"].type();

    // Print the frechet derivative type value for debugging
    std::cout << "Frechet derivative type has parsec type: " << frechet_type << std::endl;
    std::cout << "Frechet derivative type value: "
              << merged_dict["settings"]["frechet derivative type"].dump() << std::endl;

    // The bug occurs if this is an object instead of a string
    if (frechet_type != "string") {
        std::cout << "BUG DETECTED: 'frechet derivative type' is a " << frechet_type
                  << " when it should be a string" << std::endl;
        return 1;  // Test failed
    }

    // We expect this to be a string with value "finite-difference"
    std::string value = merged_dict["settings"]["frechet derivative type"].asString();
    std::cout << "Successfully got string value: " << value << std::endl;

    if (value != "finite-difference") {
        std::cout << "Unexpected value: " << value << " (expected: finite-difference)" << std::endl;
        return 1;  // Test failed
    }

    std::cout << "Test passed!" << std::endl;
    return 0;  // Test passed
}