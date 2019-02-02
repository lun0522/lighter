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
    
    template<typename PropertyType>
    void checkSupport(const vector<string> &required,
                      const function<void (uint32_t*, PropertyType*)> &enumerate,
                      const function<const char* (const PropertyType&)> &getName) {
        uint32_t count;
        enumerate(&count, nullptr);
        vector<PropertyType> properties{count};
        enumerate(&count, properties.data());
        
        unordered_set<string> available{count};
        for (const auto &prop : properties)
            available.insert(getName(prop));
        
        cout << "Available:" << endl;
        for (const auto &avl : available)
            cout << "\t" << avl << endl;
        cout << endl;
        
        cout << "Required:" << endl;
        for (const auto &req : required)
            cout << "\t" << req << endl;
        cout << endl;
        
        for (const auto &req : required) {
            if (available.find(req) == available.end())
                throw runtime_error{"Requirement not satisfied: " + req};
        }
    }
}

#endif /* utils_hpp */
