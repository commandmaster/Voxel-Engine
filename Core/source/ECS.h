#pragma once

#include <stdint.h>
#include <vector>
#include <bitset>
#include <array>
#include <limits>
#include <tuple>
#include <unordered_map>
#include <set>
#include <memory>

using IndexType = uint32_t;

struct BaseSparseSet 
{
    virtual ~BaseSparseSet() = default;
    virtual size_t getSize() const = 0;
};


template<typename Type>
struct SparseSet : public BaseSparseSet 
{
    static constexpr size_t PAGE_SIZE = 1000;
    
    std::vector<IndexType> sparseArr;
    std::vector<IndexType> elementToIdMap;
    std::vector<std::vector<Type>> denseComponents;

    size_t getSize() const override 
    {
        return elementToIdMap.size();
    }
};

struct SparseSetPtrComparator 
{
    bool operator()(const std::unique_ptr<BaseSparseSet>& a, const std::unique_ptr<BaseSparseSet>& b) const 
    {
        return a->getSize() < b->getSize();
    }
};


using EntityID = IndexType;
constexpr EntityID INVALID_ENTITY = std::numeric_limits<EntityID>::max();


class ECS
{
public:
	
private:
    std::set<std::unique_ptr<BaseSparseSet>, SparseSetPtrComparator> componentPoolSet;

};
