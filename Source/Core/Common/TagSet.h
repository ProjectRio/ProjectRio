#pragma once
#include <map>
#include <string>
#include <vector>
#include <optional>
#include <utility>

#include "HttpRequest.h"

namespace Tag
{
    class Tag
    {
    public:
        Tag() {}
        Tag(int in_id, std::string in_name, bool in_active, int in_date_created, std::string in_desc, int in_community_id, std::string in_type):
            id(in_id), 
            name(in_name),
            active(in_active),
            date_created(in_date_created),
            desc(in_desc),
            community_id(in_community_id),
            type(in_type)
            {}
        int id;
        std::string name;
        bool active;
        int date_created;
        std::string desc;
        int community_id;
        std::string type;
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
    };


    //static std::optional<picojson::value> ParseResponse(const std::vector<u8>& response)
    //{
    //    const std::string response_string(reinterpret_cast<const char*>(response.data()),response.size());
    //    picojson::value json;
    //    const auto error = picojson::parse(json, response_string);
    //    if (!error.empty())
    //        return {};
    //    return json;
    //}


    //static std::optional<TagSet> convertPicoJsonTagSet(picojson::value tag_set_pico_json) {
    //    // Initializing variables needed for constructing TagSet
    //    int id;
    //    std::string name;
    //    std::vector<Tag> tags_vector;
    //    
    //    if (tag_set_pico_json.get("ID").is<double>()){
    //        // picojson does not support the .get method for ints, so we are retrieving a double and converting to int.
    //        id = int(tag_set_pico_json.get("ID").get<double>());
    //    }
    //    else{
    //        // If ID is not a double, return an empty TagSet because that is a breaking error
    //        return std::nullopt;
    //    };

    //    if (tag_set_pico_json.get("Name").is<std::string>()){
    //        name = tag_set_pico_json.get("Name").get<std::string>();
    //    }
    //    else{
    //        // If Name is not a double, return an empty TagSet because that is a breaking error
    //        return std::nullopt;
    //    };


    //    // Get a vector of picojson::values for creating Tags 
    //    const std::vector<picojson::value> tags = tag_set_pico_json.get("Tags").get<picojson::array>();
    //    // Loop through the vector of picojson::values, 
    //    // validate that the json data meets specifications, 
    //    // and populate the empty tags_vector with Tags
    //    for (picojson::value tag : tags){
    //        // Initialize variables needed for Tag creation with default values
    //        // to use in case provided JSON data does not meet validation requirements
    //        int tag_id;
    //        std::string tag_name;
    //        bool active = false;
    //        int date_created = -1;
    //        std::string desc = "";
    //        int community_id = -1;
    //        std::string type = "";

    //        if (tag.get("ID").is<double>()){
    //            // picojson does not support the .get method for ints, so we are retrieving a double and converting to int.
    //            tag_id = int(tag.get("ID").get<double>());
    //        }
    //        else{
    //            // If ID is not a double, return an empty TagSet because that is a breaking error
    //            std::cout << "Invalid Tag ID" << "\n";
    //            return std::nullopt;
    //        }

    //        if (tag.get("Name").is<std::string>()){
    //            tag_name = tag.get("Name").get<std::string>();
    //        }
    //        else{
    //            // If Name is not a string, return an empty TagSet because that is a breaking error
    //            std::cout << "Invalid Name" << "\n";
    //            return std::nullopt;
    //        }

    //        if (tag.get("Active").is<bool>()){ active = tag.get("Active").get<bool>(); }

    //        // picojson does not support the .get method for ints, so we are retrieving a double and converting to int.
    //        if (tag.get("Date Created").is<double>()){ date_created = int(tag.get("Date Created").get<double>()); }
    //        
    //        if (tag.get("Desc").is<std::string>()){ desc = tag.get("Desc").get<std::string>(); }
    //        
    //        // picojson does not support the .get method for ints, so we are retrieving a double and converting to int.
    //        if (tag.get("Comm ID").is<double>()){ community_id = int(tag.get("Comm ID").get<double>()); }
    //        
    //        if (tag.get("Type").is<std::string>()){ tag.get("Type").get<std::string>(); }
    //        
    //        Tag tag_class = Tag{
    //            tag_id,
    //            tag_name,
    //            active,
    //            date_created,
    //            desc,
    //            community_id,
    //            type
    //        };
    //        tags_vector.push_back(tag_class);    
    //    };

    //    TagSet tag_set = TagSet{
    //        id,
    //        name,
    //        tags_vector
    //    };

    //    return tag_set;
    //}

    //static std::optional<TagSet> getTagSet(Common::HttpRequest &http, int tag_set_id){
    //    // const Common::HttpRequest::Response response = m_http.Get("https://api.projectrio.app/tag_set/" + std::to_string(tag_set_id));
    //    const Common::HttpRequest::Response response = http.Get("http://127.0.0.1:5000/tag_set/" + std::to_string(tag_set_id));

    //    if (!response){
    //        std::cout << "No Response" << "\n";
    //        return std::nullopt;
    //    }

    //    auto json = ParseResponse(response.value());
    //    if (!json){
    //        std::cout << "No JSON" << "\n"; 
    //        return std::nullopt;
    //    }
    //    
    //    picojson::value tag_set_pico_json = json->get("Tag Set").get<picojson::array>()[0];

    //    std::optional<TagSet> tag_set = convertPicoJsonTagSet(tag_set_pico_json);
    //    
    //    return tag_set;
    //}

    //static std::optional<TagSet> getDummyTagSet() {
    //    return TagSet(
    //        1,
    //        "Dummy Tag Set 1",
    //        {
    //            Tag(
    //                1,
    //                "Fake Tag 1",
    //                true,
    //                19419849,
    //                "Fake Description",
    //                1,
    //                "Competition"
    //            ), 
    //            Tag(
    //                2,
    //                "Fake Tag 2",
    //                true,
    //                19419849,
    //                "Fake Description",
    //                1,
    //                "Competition"
    //            ), 
    //        }
    //    );
    //}

    //static std::map<int, TagSet> getAvailableTagSets(Common::HttpRequest &http, std::string rio_key){
    //    std::stringstream sstm;
    //    sstm << "{\"Active\":\"true\", \"Rio Key\":\"" << rio_key << "\"}";
    //    std::string payload = sstm.str(); 
    //    const Common::HttpRequest::Response response = http.Post(
    //        "http://127.0.0.1:5000/tag_set/list",
    //        payload,
    //        {{"Content-Type", "application/json"},}
    //    );

    //    if (!response){
    //        std::cout << "No Response" << "\n";
    //        std::map<int, TagSet> empty_map;
    //        return empty_map;
    //    }

    //    auto json = ParseResponse(response.value());
    //    if (!json){
    //        std::cout << "No JSON" << "\n"; 
    //        std::map<int, TagSet> empty_map;
    //        return empty_map;
    //    }

    //    // Initialize vector that will be populated with TagSets and returned at end of function
    //    std::map<int, TagSet> tag_sets;

    //    // Create a vector of tag_sets as picojson objects
    //    std::vector<picojson::value> tag_sets_pico_json = json->get("Tag Sets").get<picojson::array>();

    //    // Loop through each pico json tag set object
    //    for (picojson::value tag_set_pico_json : tag_sets_pico_json){
    //        std::optional<TagSet> tag_set = convertPicoJsonTagSet(tag_set_pico_json);     

    //        // If tag_set is valid, add to tag_sets vector
    //        if (tag_set) {                
    //            tag_sets.insert(std::make_pair(tag_set.value().id, tag_set.value()));
    //        }
    //    }
    //    
    //    return tag_sets;
    //}

    //static std::map<int, TagSet> getDummyTagSets() {
    //    TagSet tag_set_a = TagSet(
    //        1,
    //        "Dummy Tag Set 1",
    //        {
    //            Tag(
    //                1,
    //                "Fake Tag 1",
    //                true,
    //                19419849,
    //                "Fake Description",
    //                1,
    //                "Competition"
    //            ), 
    //            Tag(
    //                2,
    //                "Fake Tag 2",
    //                true,
    //                19419849,
    //                "Fake Description",
    //                1,
    //                "Competition"
    //            ), 
    //        }
    //    );

    //    TagSet tag_set_b = TagSet(
    //        2,
    //        "Dummy Tag Set 2",
    //        {
    //            Tag(
    //                1,
    //                "Fake Tag",
    //                true,
    //                19419849,
    //                "Fake Description",
    //                1,
    //                "Competition"
    //            ),
    //            Tag(
    //                2,
    //                "Fake Tag 2",
    //                true,
    //                19419849,
    //                "Fake Description",
    //                1,
    //                "Competition"
    //            ),
    //        }
    //    );

    //    std::map<int, TagSet> dummy_tag_sets;

    //    dummy_tag_sets.insert(std::make_pair(tag_set_a.id, tag_set_a));
    //    dummy_tag_sets.insert(std::make_pair(tag_set_b.id, tag_set_b));

    //    return dummy_tag_sets;
    //}
}
