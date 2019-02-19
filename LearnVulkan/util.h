//
//  util.h
//  LearnVulkan
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LEARNVULKAN_UTIL_H
#define LEARNVULKAN_UTIL_H

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#define ASSERT_NONNULL(object, error) \
    if (object == nullptr)      throw std::runtime_error{error}
#define ASSERT_SUCCESS(event, error) \
    if (event != VK_SUCCESS)    throw std::runtime_error{error}
#define CONTAINER_SIZE(container)   static_cast<uint32_t>(container.size())

namespace util {

using namespace std;

template<typename AttribType>
vector<AttribType> QueryAttribute(
    const function<void (uint32_t*, AttribType*)>& enumerate) {
    uint32_t count;
    enumerate(&count, nullptr);
    vector<AttribType> attribs{count};
    enumerate(&count, attribs.data());
    return attribs;
}

template<typename AttribType>
void CheckSupport(
    const vector<string>& required,
    const vector<AttribType>& attribs,
    const function<const char* (const AttribType&)>& get_name) {
    unordered_set<string> available{attribs.size()};
    for (const auto& atr : attribs)
        available.insert(get_name(atr));
    
    cout << "Available:" << endl;
    for (const auto& avl : available)
        cout << "\t" << avl << endl;
    cout << endl;
    
    cout << "Required:" << endl;
    for (const auto& req : required)
        cout << "\t" << req << endl;
    cout << endl;
    
    for (const auto& req : required) {
        if (available.find(req) == available.end())
            throw runtime_error{"Requirement not satisfied: " + req};
    }
}

template <typename ContentType, typename Predicate>
bool FindFirst(const vector<ContentType>& container,
               Predicate predicate,
               uint32_t& first) {
    auto first_itr = find_if(container.begin(), container.end(), predicate);
    first = static_cast<uint32_t>(distance(container.begin(), first_itr));
    return first_itr != container.end();
}

const string& ReadFile(const string& path);

}  /* namespace util */

#endif /* LEARNVULKAN_UTIL_H */
