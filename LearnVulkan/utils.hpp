//
//  utils.hpp
//  LearnVulkan
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef utils_hpp
#define utils_hpp

#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace Utils {
    using namespace std;
    
    void checkRequirements(const unordered_set<string> &available,
                           const vector<string> &required) {
        for (const auto& req : required) {
            if (available.find(req) == available.end())
                throw runtime_error{"Requirement not satisfied: " + req};
        }
    }
    
    template<typename PropertyType>
    void checkSupport(const vector<string> &required,
                      const function<void (uint32_t*, PropertyType*)> &enumerate,
                      const function<const char* (const PropertyType&)> &getName) {
        uint32_t count;
        enumerate(&count, nullptr);
        vector<PropertyType> properties{count};
        enumerate(&count, properties.data());
        
        unordered_set<string> available{count};
        cout << "Available:" << endl;
        for (const auto &prop : properties) {
            string name{getName(prop)};
            cout << "\t" << name << endl;
            available.insert(name);
        }
        cout << endl;
        
        cout << "Required:" << endl;
        for (const auto &req : required)
            cout << "\t" << req << endl;
        cout << endl;
        
        checkRequirements(available, required);
    }
}

#endif /* utils_hpp */
