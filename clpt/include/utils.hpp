//
// Created by taylor-santos on 6/11/2022 at 16:27.
//

#pragma once

/*
 * In-place construct a new element into a map, erasing the old element if the key matches.
 */
template<typename Key, typename Value, typename... Args>
auto
overwrite_emplace(std::map<Key, Value> &map, const Key &key, Args &&...args) {
    // Use lower_bound to find the first element either matching the key or directly after where it
    // would be inserted. This can either be used to erase the existing element or be used as a
    // location hint for try_emplace.
    auto hint = map.lower_bound(key);
    // Check if the key already exists in the map, and delete it if so.
    if (hint != map.end() && hint->first == key) {
        // Post-increment iterator first so it points to the next element after the call to erase().
        // std::map.erase() doesn't invalidate iterators to elements not being erased.
        map.erase(hint++);
    }
    // Finally, construct the new element in-place in the map, given the hint.
    return map.try_emplace(hint, key, args...);
}
