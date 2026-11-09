#pragma once

#include <picojson.h>
#include <sstream>
#include <map>
#include <string>
#include <vector>
#include <optional>
#include <utility>

#include <picojson.h>
#include "sstream"

#include "Common/HttpRequest.h"

enum ClientCode {
    DisableManualFielderSelect,
    DisableAntiQuickPitch,
    DisableUnlimitedExtraInnings,
    DisableSuperstars,
    DisableStarSkills,
    DisableAntiDingusBunt,
    EnableHazardless,
    EnableGameModeration,
    DefaultNineInnings,
    DefaultDropSpotsOff,
    DuplicateCharacters,
    DuplicateChem
};

namespace Tag
{
    class Tag
    {
    public:
        Tag() {}
        Tag(int in_id, std::string in_name, bool in_active, int in_date_created, std::string in_desc, int in_community_id, std::string in_type, std::optional<ClientCode> in_client_code, std::optional<std::string> in_gecko_code):
            id(in_id), 
            name(in_name),
            active(in_active),
            date_created(in_date_created),
            desc(in_desc),
            community_id(in_community_id),
            type(in_type),
            client_code(in_client_code),
            gecko_code(in_gecko_code)
            {}
        int id;
        std::string name;
        bool active;
        int date_created;
        std::string desc;
        int community_id;
        std::string type;
        std::optional<ClientCode> client_code;
        std::optional<std::string> gecko_code;
    };


    class TagSet
    {
    public:
        TagSet() {}
        TagSet(int in_id, std::string in_name, std::vector<Tag> in_tags):
            id(in_id),
            name(in_name),
            tags(in_tags)
            {}

        int id;
        std::string name;
        std::vector<Tag> tags;

        std::vector<std::string> tag_names_vector()
        {
            std::vector<std::string> names;
            for (Tag tag : tags) {
                names.push_back(tag.name);
            }
            return names;
        }

        std::vector<int> tag_ids_vector()
        {
            std::vector<int> ids;
            for (Tag tag : tags) {
                ids.push_back(tag.id);
            }
            return ids;
        }

        std::string tag_names_string()
        {
            std::stringstream sstm;
            sstm << "[";
            for (Tag tag: tags) {
                sstm << "\"" << tag.name << "\", ";
            }
            std::string names = sstm.str();
            // Remove trailing characters ", " and add closing brace
            names.pop_back();
            names.pop_back();
            names += "]";
            return names;
        }

        std::string tag_ids_string()
        {
            std::stringstream sstm;
            sstm << "[";
            for (Tag tag: tags) {
                sstm << tag.id << ", ";
            }
            std::string ids = sstm.str();
            // Remove trailing characters ", " and add closing brace
            ids.pop_back();
            ids.pop_back();
            ids += "]";
            return ids;
        }

        std::string description()
        {
            std::stringstream sstm;
            for (Tag tag: tags) {
                if (tag.type == "Competition") { sstm << tag.desc; }
            }
            return sstm.str();
        }

        // Create vector of client code enums
        std::optional<std::vector<ClientCode>> client_codes_vector() 
        {
            std::vector<ClientCode> client_codes = {};
            for (Tag tag : tags) {
                if (tag.client_code.has_value()) {
                    client_codes.push_back(tag.client_code.value());
                }
            }

            if (client_codes.size() != 0) {
                return client_codes;
            } else {
                return std::nullopt;
            }
            
        }

        // Create monster code string
        std::optional<std::vector<std::string>> gecko_codes_string() 
        {
            std::vector<std::string> code_segments;
            std::vector<Tag>::const_iterator it;
            
            // iterate throught TagSet's Tags and create a vector of code segments (half a code, a string u32)
            for (it = tags.begin(); it != tags.end(); it++) {
                if (it->gecko_code.has_value()) {
                    //sstm << it->gecko_code.value();
                    std::string full_code = it->gecko_code.value();
                    size_t line_pos = 0;
                    std::string code_line;

                    //First seperate line by line
                    std::string line_delimiter = "\n";
                    while ((line_pos = full_code.find(line_delimiter)) != std::string::npos) {
                        code_line = full_code.substr(0, line_pos);
                        std::cout << code_line << std::endl;
                        full_code.erase(0, line_pos + line_delimiter.length());

                        ////Then seperate each line into a single word of code
                        //std::string half_line_delimiter = " ";
                        //size_t half_line_pos = 0;
                        //std::string half_line;
                        //while ((half_line_pos = code_line.find(half_line_delimiter)) != std::string::npos) {
                        //    half_line = code_line.substr(0, half_line_pos);
                        //    std::cout << half_line << std::endl;
                        //    code_segments.push_back(half_line);
                        //    code_line.erase(0, half_line_pos + half_line_delimiter.length());
                        //}
                        code_segments.push_back(code_line); //Last half is accessed outside of loop
                    }
                }
            }

            // if the vector is not empty return string of gecko codes
            if (code_segments.size() > 0) {
                return code_segments;
            // if vector is empty, return nullopt
            } else {
                return std::nullopt;
            }
        }
    };


    static std::optional<picojson::value> ParseResponse(const std::vector<u8>& response)
    {
       const std::string response_string(reinterpret_cast<const char*>(response.data()),response.size());
       picojson::value json;
       const auto error = picojson::parse(json, response_string);
       if (!error.empty())
           return {};
       return json;
    }

    static std::optional<ClientCode> getTagClientCode(std::string tag_name, bool is_client_code) {
        if (!is_client_code){
            return std::nullopt;
        }
        
        if (tag_name == "Disable Anti Dingus Bunt") {
            return ClientCode::DisableAntiDingusBunt;
        } else if (tag_name == "Disable Manual Fielder Select") {
            return ClientCode::DisableManualFielderSelect;
        } else if (tag_name == "Disable Anti Quick Pitch") {
            return ClientCode::DisableAntiQuickPitch;
        } else if (tag_name == "Disable Unlimited Extra Innings") {
            return ClientCode::DisableUnlimitedExtraInnings;
        } else if (tag_name == "Enable Hazardless") {
            return ClientCode::EnableHazardless;
        } else if (tag_name == "Enable Game Moderation") {
            return ClientCode::EnableGameModeration;
        } else if (tag_name == "Disable Superstars") {
            return ClientCode::DisableSuperstars;
        } else if (tag_name == "Disable Star Skills") {
            return ClientCode::DisableStarSkills;
        } else if (tag_name == "Default Nine Innings") {
            return ClientCode::DefaultNineInnings;
        } else if (tag_name == "Default Drop Spots Off") {
            return ClientCode::DefaultDropSpotsOff;
        } else if (tag_name == "Duplicate Characters") {
            return ClientCode::DuplicateCharacters;
        } else if (tag_name == "Duplicates and Variants Have Chem") {
            return ClientCode::DuplicateChem;
        } else {                                
            return std::nullopt;
        }
    }


    static std::optional<TagSet> convertPicoJsonTagSet(picojson::value tag_set_pico_json) {
        // Initializing variables needed for constructing TagSet
        int id;
        std::string name;
        std::vector<Tag> tags_vector;
        
        if (tag_set_pico_json.get("id").is<double>()){
            // picojson does not support the .get method for ints, so we are retrieving a double and converting to int.
            id = int(tag_set_pico_json.get("id").get<double>());
        }
        else{
            // If ID is not a double, return an empty TagSet because that is a breaking error
            return std::nullopt;
        };

        if (tag_set_pico_json.get("name").is<std::string>()){
            name = tag_set_pico_json.get("name").get<std::string>();
        }
        else{
            // If Name is not a double, return an empty TagSet because that is a breaking error
            return std::nullopt;
        };

        std::cout << "So far so good" << "\n";

        // Get a vector of picojson::values for creating Tags 
        const std::vector<picojson::value> tags = tag_set_pico_json.get("tags").get<picojson::array>();
        // Loop through the vector of picojson::values, 
        // validate that the json data meets specifications, 
        // and populate the empty tags_vector with Tags
        for (picojson::value tag : tags){
            // Initialize variables needed for Tag creation with default values
            // to use in case provided JSON data does not meet validation requirements
            int tag_id;
            std::string tag_name;
            bool active = false;
            int date_created = -1;
            std::string desc = "";
            int community_id = -1;
            std::string type = "";
            std::optional<ClientCode> client_code = std::nullopt;
            std::optional<std::string> gecko_code = std::nullopt;

            if (tag.get("id").is<double>()){
                // picojson does not support the .get method for ints, so we are retrieving a double and converting to int.
                tag_id = int(tag.get("id").get<double>());
            }
            else{
                // If ID is not a double, return an empty TagSet because that is a breaking error
                std::cout << "Invalid Tag ID" << "\n";
                return std::nullopt;
            }

            if (tag.get("name").is<std::string>()){
                tag_name = tag.get("name").get<std::string>();
            }
            else{
                // If Name is not a string, return an empty TagSet because that is a breaking error
                std::cout << "Invalid Name" << "\n";
                return std::nullopt;
            }

            if (tag.get("active").is<bool>()){ active = tag.get("active").get<bool>(); }

            // picojson does not support the .get method for ints, so we are retrieving a double and converting to int.
            if (tag.get("date_created").is<double>()){ date_created = int(tag.get("date_created").get<double>()); }
            
            if (tag.get("desc").is<std::string>()){ desc = tag.get("desc").get<std::string>(); }
            
            // picojson does not support the .get method for ints, so we are retrieving a double and converting to int.
            if (tag.get("comm_id").is<double>()){ community_id = int(tag.get("comm_id").get<double>()); }
            
            if (tag.get("type").is<std::string>()){ type = tag.get("type").get<std::string>(); }

            // Check if the tag's name is one of the Client Codes
            client_code = getTagClientCode(tag_name, (type == "Client Code"));

            // Check if the Tag has a Gecko Code
            if (tag.get("gecko_code").is<std::string>()){ 
                std::string code = tag.get("gecko_code").get<std::string>();

                // If the code field is not empty, set Tag.gecko_code to the code
                if (code != "") {
                    gecko_code = code;
                }
            }
            
            Tag tag_class = Tag{
                tag_id,
                tag_name,
                active,
                date_created,
                desc,
                community_id,
                type,
                client_code,
                gecko_code
            };
            tags_vector.push_back(tag_class);    
        };

        TagSet tag_set = TagSet{
            id,
            name,
            tags_vector
        };

        return tag_set;
    }

    static inline std::optional<TagSet> getTagSet(Common::HttpRequest &http, int tag_set_id){
       // const Common::HttpRequest::Response response = m_http.Get("https://api.projectrio.app/tag_set/" + std::to_string(tag_set_id));
       const Common::HttpRequest::Response response = http.Get("https://api.projectrio.app/tag_set/" + std::to_string(tag_set_id));

       if (!response){
           std::cout << "No Response" << "\n";
           return std::nullopt;
       }

       auto json = ParseResponse(response.value());
       if (!json){
           std::cout << "No JSON" << "\n"; 
           return std::nullopt;
       }
       
       picojson::value tag_set_pico_json = json->get("Tag Set").get<picojson::array>()[0];

       std::optional<TagSet> tag_set = convertPicoJsonTagSet(tag_set_pico_json);
       
       return tag_set;
    }

    static inline std::optional<TagSet> getDummyTagSet() {
       return TagSet(
           1,
           "Dummy Tag Set 1",
           {
               Tag(
                    1,
                    "Fake Client Code Tag 1",
                    true,
                    19419849,
                    "Fake Description",
                    1,
                    "Competition",
                    ClientCode::EnableHazardless,
                    std::nullopt
               ), 
               Tag(
                    2,
                    "Fake Competition Tag 1",
                    true,
                    19419849,
                    "Fake Description",
                    1,
                    "Competition",
                    std::nullopt,
                    std::nullopt
               ),
               Tag(
                    3,
                    "Fake Gecko Code Tag 1",
                    true,
                    19419849,
                    "Fake Description",
                    1,
                    "Competition",
                    std::nullopt,
                    "0123456 0123456"
                ),
                Tag(
                    4,
                    "Fake Gecko Code Tag 2",
                    true,
                    19419849,
                    "Fake Description",
                    1,
                    "Code",
                    std::nullopt,
                    "DEADBEEF DEADBEEF"
                ),
           }
       );
    }

    static inline std::map<int, TagSet> getAvailableTagSets(Common::HttpRequest &http, std::string rio_key){
       std::stringstream sstm;
       sstm << "{\"Active\":\"true\", \"Rio Key\":\"" << rio_key << "\"}";
       std::string payload = sstm.str(); 
       const Common::HttpRequest::Response response = http.Post(
           "https://api.projectrio.app/tag_set/list",
           payload,
           {{"Content-Type", "application/json"},}
       );

       if (!response){
           std::cout << "No Response" << "\n";
           std::map<int, TagSet> empty_map;
           return empty_map;
       }

       auto json = ParseResponse(response.value());
       if (!json){
           std::cout << "No JSON" << "\n"; 
           std::map<int, TagSet> empty_map;
           return empty_map;
       }

       // Initalize vector that will be populated with TagSets and returned at end of function
       std::map<int, TagSet> tag_sets;

       // Create a vector of tag_sets as picojson objects
       std::vector<picojson::value> tag_sets_pico_json = json->get("Tag Sets").get<picojson::array>();

       // Loop through each pico json tag set object
       for (picojson::value tag_set_pico_json : tag_sets_pico_json){
           std::optional<TagSet> tag_set = convertPicoJsonTagSet(tag_set_pico_json);     

           // If tag_set is valid, add to tag_sets vector
           if (tag_set) {                
               tag_sets.insert(std::make_pair(tag_set.value().id, tag_set.value()));
           }
       }
       
       return tag_sets;
    }

    static inline std::map<int, TagSet> getDummyTagSets() {
       TagSet tag_set_a = TagSet(
           1,
           "Dummy Tag Set 1",
           {
                Tag(
                    1,
                    "Fake Client Code Tag 1",
                    true,
                    19419849,
                    "Fake Description",
                    1,
                    "Competition",
                    ClientCode::EnableHazardless,
                    std::nullopt
                ), 
                Tag(
                    1,
                    "Fake Client Code Tag 1",
                    true,
                    19419849,
                    "Fake Description",
                    1,
                    "Competition",
                    ClientCode::DisableAntiQuickPitch,
                    std::nullopt
                ), 
                Tag(
                    2,
                    "Fake Competition Tag 1",
                    true,
                    19419849,
                    "Fake Description",
                    1,
                    "Competition",
                    std::nullopt,
                    std::nullopt
                ),
                Tag(
                    3,
                    "Fake Gecko Code Tag 1",
                    true,
                    19419849,
                    "Fake Description",
                    1,
                    "Competition",
                    std::nullopt,
                    "0123456 0123456"
                ),
                Tag(
                    4,
                    "Fake Gecko Code Tag 2",
                    true,
                    19419849,
                    "Fake Description",
                    1,
                    "Code",
                    std::nullopt,
                    "DEADBEEF DEADBEEF"
                ),
           }
       );

       TagSet tag_set_b = TagSet(
           2,
           "Dummy Tag Set 2",
           {
               Tag(
                    1,
                    "Fake Client Code Tag 1",
                    true,
                    19419849,
                    "Fake Description",
                    1,
                    "Competition",
                    ClientCode::EnableHazardless,
                    std::nullopt
               ), 
                Tag(
                    1,
                    "Fake Client Code Tag 1",
                    true,
                    19419849,
                    "Fake Description",
                    1,
                    "Competition",
                    ClientCode::DisableAntiQuickPitch,
                    std::nullopt
                ), 
               Tag(
                    2,
                    "Fake Competition Tag 1",
                    true,
                    19419849,
                    "Fake Description",
                    1,
                    "Competition",
                    std::nullopt,
                    std::nullopt
               ),
               Tag(
                    3,
                    "Fake Gecko Code Tag 1",
                    true,
                    19419849,
                    "Fake Description",
                    1,
                    "Competition",
                    std::nullopt,
                    "0123456 0123456"
                ),
                Tag(
                    4,
                    "Fake Gecko Code Tag 2",
                    true,
                    19419849,
                    "Fake Description",
                    1,
                    "Code",
                    std::nullopt,
                    "DEADBEEF DEADBEEF"
                ),
           }
       );

       std::map<int, TagSet> dummy_tag_sets;

       dummy_tag_sets.insert(std::make_pair(tag_set_a.id, tag_set_a));
       dummy_tag_sets.insert(std::make_pair(tag_set_b.id, tag_set_b));

       return dummy_tag_sets;
    }
}
