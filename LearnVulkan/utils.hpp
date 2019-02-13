//
//  utils.hpp
//  LearnVulkan
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef utils_hpp
#define utils_hpp

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#define ASSERT_NONNULL(object, error)   if (object == nullptr)      throw std::runtime_error{error}
#define ASSERT_SUCCESS(event, error)    if (event != VK_SUCCESS)    throw std::runtime_error{error}

namespace Utils {
    using namespace std;
    
    template<typename AttribType>
    vector<AttribType> queryAttribute(const function<void (uint32_t*, AttribType*)> &enumerate) {
        uint32_t count;
        enumerate(&count, nullptr);
        vector<AttribType> attribs{count};
        enumerate(&count, attribs.data());
        return attribs;
    }
    
    template<typename AttribType>
    void checkSupport(const vector<string> &required,
                      const vector<AttribType> &attribs,
                      const function<const char* (const AttribType&)> &getName) {
        unordered_set<string> available{attribs.size()};
        for (const auto &atr : attribs)
            available.insert(getName(atr));
        
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
    
    template <typename ContentType, typename Predicate>
    bool findFirst(const vector<ContentType> &container, Predicate predicate, uint32_t &first) {
        auto firstIter = find_if(container.begin(), container.end(), predicate);
        first = static_cast<uint32_t>(distance(container.begin(), firstIter));
        return firstIter != container.end();
    }
}

#endif /* utils_hpp */
