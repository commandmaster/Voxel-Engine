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
#include <stdexcept>
#include <string>
#include <functional>
#include <iostream>
#include <utility>
#include <type_traits>
#include <string_view>
#include <any>

using IndexType = uint64_t;
constexpr IndexType INVALID_INDEX = std::numeric_limits<IndexType>::max();


using EntityID = IndexType;
constexpr EntityID INVALID_ENTITY = INVALID_INDEX;


struct BaseSparseSet 
{
    virtual ~BaseSparseSet() = default;
    virtual size_t getSize() const = 0;
    virtual bool contains(IndexType id) const = 0;
    virtual void clear() = 0;
    virtual bool deleteEntry(IndexType id) = 0;
    virtual const std::vector<IndexType>& getEntityIds() const = 0;
};


template<typename Type>
struct SparseSet : public BaseSparseSet 
{
    SparseSet() {}
    std::vector<IndexType> sparseArr;
    std::vector<IndexType> elementToIdMap;
    std::vector<Type> denseComponents;

    bool contains(IndexType id) const override
    {
        if (id >= sparseArr.size()) return false;

        IndexType denseIndex = sparseArr[id];
		return denseIndex != INVALID_INDEX && denseIndex < elementToIdMap.size();       
    }

    size_t getSize() const override 
    {
        return elementToIdMap.size();
    }

    void clear() override
    {
        sparseArr.clear();
        elementToIdMap.clear();
        denseComponents.clear();
    }

    bool deleteEntry(IndexType id) override 
    {
		if (!contains(id)) return false;

		IndexType componentIndex = sparseArr[id];
		bool isLastElement = (componentIndex == elementToIdMap.size() - 1);

		if (!isLastElement) 
        {
			elementToIdMap[componentIndex] = elementToIdMap.back();
            denseComponents[componentIndex] = std::move(denseComponents.back());

			sparseArr[elementToIdMap[componentIndex]] = componentIndex;
		}

		elementToIdMap.pop_back();
		denseComponents.pop_back();

		sparseArr[id] = INVALID_INDEX;

		return true;
	}

    void addComponent(IndexType id, const Type& component)
    {
        if (id >= sparseArr.size()) 
        {
			size_t required_size = id + 1;
			sparseArr.resize(required_size, INVALID_INDEX);
		}

        if (!contains(id))
        {
			elementToIdMap.push_back(id);
			denseComponents.push_back(component);
			sparseArr[id] = static_cast<IndexType>(elementToIdMap.size() - 1);
        }
        else
        {
            throw std::runtime_error("Id is already contained in set");
        }
    }

    void addComponent(IndexType id, Type&& component)
    {
        if (id >= sparseArr.size())
        {
            size_t required_size = id + 1;
            sparseArr.resize(required_size, INVALID_INDEX);
        }

        if (!contains(id))
        {
            IndexType newDenseIndex = static_cast<IndexType>(elementToIdMap.size());
            elementToIdMap.push_back(id);
            denseComponents.push_back(std::move(component)); // Move component
            sparseArr[id] = newDenseIndex;
        }
        else
        {
            throw std::runtime_error("Id " + std::to_string(id) + " is already contained in set");
        }
    }

    void setComponent(IndexType id, const Type& component)
    {
        if (!contains(id))
        {
            throw std::out_of_range("Id " + std::to_string(id) + " not found in SparseSet::setComponent");
        }
        IndexType denseIndex = sparseArr[id];
        denseComponents[denseIndex] = component;
    }

    void setComponent(IndexType id, Type&& component)
    {
        if (!contains(id))
        {
            throw std::out_of_range("Id " + std::to_string(id) + " not found in SparseSet::setComponent");
        }
        IndexType denseIndex = sparseArr[id];
        denseComponents[denseIndex] = std::move(component); 
    }

    Type& getEntry(IndexType id)
    {
        if (!contains(id))
        {
             throw std::out_of_range("Id " + std::to_string(id) + " not found in SparseSet::getEntry");
        }
        IndexType denseIndex = sparseArr[id];
        return denseComponents[denseIndex];
    }

    const Type& getEntry(IndexType id) const
    {
        if (!contains(id))
        {
            throw std::out_of_range("Id " + std::to_string(id) + " not found in SparseSet::getEntry (const)");
        }
        IndexType denseIndex = sparseArr[id];
        return denseComponents[denseIndex];
    }

    Type* getEntryPtr(IndexType id)
    {
        if (!contains(id)) 
        {
            return nullptr;
        }
        IndexType denseIndex = sparseArr[id];
        return &denseComponents[denseIndex];
    }

    const Type* getEntryPtr(IndexType id) const
    {
        if (!contains(id)) 
        {
            return nullptr;
        }
        IndexType denseIndex = sparseArr[id];
        return &denseComponents[denseIndex];
    }

    const std::vector<IndexType>& getEntityIds() const override
    {
        return elementToIdMap;
    }
};


template<typename... ComponentTypes>
struct MultiSparseSet : public BaseSparseSet
{
    static_assert(sizeof...(ComponentTypes) > 0, "MultiSparseSet requires at least one component type.");


    std::vector<IndexType> sparseArr;
    std::vector<IndexType> elementToIdMap; 
    std::tuple<std::vector<ComponentTypes>...> denseComponents; 

    MultiSparseSet() = default; 

    bool contains(IndexType id) const override
    {
        if (id >= sparseArr.size()) return false;
        IndexType denseIndex = sparseArr[id];
        return denseIndex != INVALID_INDEX && denseIndex < elementToIdMap.size();
    }

    size_t getSize() const override
    {
        return elementToIdMap.size();
    }

    void clear() override
    {
        sparseArr.clear();
        elementToIdMap.clear();
        std::apply([](auto&... vecs) { (vecs.clear(), ...); }, denseComponents);
    }

    template<size_t I>
    void deleteSwapPop(IndexType denseIndex) 
    {
        if (denseIndex < std::get<I>(denseComponents).size() - 1) 
        {
             std::get<I>(denseComponents)[denseIndex] = std::move(std::get<I>(denseComponents).back());
        }
        if (!std::get<I>(denseComponents).empty()) 
        {
            std::get<I>(denseComponents).pop_back();
        }
    }

    template <std::size_t... Is>
    void deleteAllComponentsAtIndex(IndexType denseIndex, std::index_sequence<Is...>) 
    {
        (deleteSwapPop<Is>(denseIndex), ...); // C++17 fold expression
    }


    bool deleteEntry(IndexType id) override
    {
        if (!contains(id)) return false;

        IndexType denseIndexToDelete = sparseArr[id];
        IndexType lastElementId = elementToIdMap.back(); 

        deleteAllComponentsAtIndex(denseIndexToDelete, std::index_sequence_for<ComponentTypes...>{});

        if (denseIndexToDelete < elementToIdMap.size() -1) 
        {
            elementToIdMap[denseIndexToDelete] = lastElementId;
            sparseArr[lastElementId] = denseIndexToDelete;
        }

         if (!elementToIdMap.empty()) {
            elementToIdMap.pop_back();
         }

        sparseArr[id] = INVALID_INDEX;

        return true;
    }

    const std::vector<IndexType>& getEntityIds() const override
    {
        return elementToIdMap;
    }

	void addComponentMulti(IndexType id, const ComponentTypes&... components)
    {
        addComponentInternal(id, components...);
    }

    void addComponentMulti(IndexType id, ComponentTypes&&... components)
    {
        addComponentInternal(id, std::move(components)...);
    }

	void setComponentMulti(IndexType id, const ComponentTypes&... components)
    {
       setComponentInternal(id, components...);
    }

    void setComponentMulti(IndexType id, ComponentTypes&&... components)
    {
        setComponentInternal(id, std::move(components)...);
    }

	std::tuple<ComponentTypes&...> getEntry(IndexType id)
    {
        if (!contains(id))
        {
             throw std::out_of_range("Id " + std::to_string(id) + " not found in MultiSparseSet::getEntry");
        }
        IndexType denseIndex = sparseArr[id];
        return getEntryTuple(denseIndex, std::index_sequence_for<ComponentTypes...>{});
    }

    std::tuple<const ComponentTypes&...> getEntry(IndexType id) const
    {
        if (!contains(id))
        {
            throw std::out_of_range("Id " + std::to_string(id) + " not found in MultiSparseSet::getEntry (const)");
        }
        IndexType denseIndex = sparseArr[id];
        return getEntryTupleConst(denseIndex, std::index_sequence_for<ComponentTypes...>{});
    }

	std::tuple<ComponentTypes*...> getEntryPtr(IndexType id)
    {
        if (!contains(id))
        {
            return std::apply([](auto... args) { return std::make_tuple(((decltype(args)*)nullptr)...); },
                              std::tuple<ComponentTypes...>{});
        }
        IndexType denseIndex = sparseArr[id];
        return getEntryPtrTuple(denseIndex, std::index_sequence_for<ComponentTypes...>{});
    }

    std::tuple<const ComponentTypes*...> getEntryPtr(IndexType id) const
    {
        if (!contains(id))
        {
            return std::apply([](auto... args) { return std::make_tuple(((const decltype(args)*)nullptr)...); },
                              std::tuple<ComponentTypes...>{});
        }
        IndexType denseIndex = sparseArr[id];
        return getEntryPtrTupleConst(denseIndex, std::index_sequence_for<ComponentTypes...>{});
    }


private:
    template<size_t I, typename ComponentArg>
    void addComponentToVector(ComponentArg&& component) 
    {
         std::get<I>(denseComponents).push_back(std::forward<ComponentArg>(component));
    }

    template<typename... ComponentArgs, std::size_t... Is>
    void addAllComponents(std::index_sequence<Is...>, ComponentArgs&&... components) 
    {
        (addComponentToVector<Is>(std::forward<ComponentArgs>(components)), ...);
    }

    template<typename... ComponentArgs>
    void addComponentInternal(IndexType id, ComponentArgs&&... components)
    {
        static_assert(sizeof...(ComponentArgs) == sizeof...(ComponentTypes), "Incorrect number of components provided to addComponentInternal");

        if (id >= sparseArr.size())
        {
            size_t required_size = id + 1;
            sparseArr.resize(required_size, INVALID_INDEX);
        }

        if (!contains(id))
        {
            IndexType newDenseIndex = static_cast<IndexType>(elementToIdMap.size());
            elementToIdMap.push_back(id);
            addAllComponents(std::index_sequence_for<ComponentTypes...>{}, std::forward<ComponentArgs>(components)...);
            sparseArr[id] = newDenseIndex;
        }
        else
        {
            throw std::runtime_error("Id " + std::to_string(id) + " is already contained in MultiSparseSet");
        }
    }

    template<size_t I, typename ComponentArg>
    void setComponentInVector(IndexType denseIndex, ComponentArg&& component) 
    {
         std::get<I>(denseComponents)[denseIndex] = std::forward<ComponentArg>(component);
    }

    template<typename... ComponentArgs, std::size_t... Is>
    void setAllComponents(IndexType denseIndex, std::index_sequence<Is...>, ComponentArgs&&... components) 
    {
        (setComponentInVector<Is>(denseIndex, std::forward<ComponentArgs>(components)), ...);
    }

    template<typename... ComponentArgs>
    void setComponentInternal(IndexType id, ComponentArgs&&... components)
    {
        static_assert(sizeof...(ComponentArgs) == sizeof...(ComponentTypes), "Incorrect number of components provided to setComponentInternal");

        if (!contains(id))
        {
            throw std::out_of_range("Id " + std::to_string(id) + " not found in MultiSparseSet::setComponent");
        }
        IndexType denseIndex = sparseArr[id];
        setAllComponents(denseIndex, std::index_sequence_for<ComponentTypes...>{}, std::forward<ComponentArgs>(components)...);
    }

    template<size_t I>
    decltype(auto) getComponentRef(IndexType denseIndex) 
    {
        return (std::get<I>(denseComponents)[denseIndex]);
    }

    template<size_t I>
    decltype(auto) getComponentRefConst(IndexType denseIndex) const 
    {
         return (std::get<I>(denseComponents)[denseIndex]);
    }

    template <std::size_t... Is>
    std::tuple<ComponentTypes&...> getEntryTuple(IndexType denseIndex, std::index_sequence<Is...>) 
    {
        return std::tie(getComponentRef<Is>(denseIndex)...);
    }

    template <std::size_t... Is>
    std::tuple<const ComponentTypes&...> getEntryTupleConst(IndexType denseIndex, std::index_sequence<Is...>) const 
    {
        return std::tie(getComponentRefConst<Is>(denseIndex)...);
    }

    template<size_t I>
    auto* getComponentPtrHelper(IndexType denseIndex) 
    {
        return &(std::get<I>(denseComponents)[denseIndex]);
    }

    template<size_t I>
    const auto* getComponentPtrHelperConst(IndexType denseIndex) const 
    {
         return &(std::get<I>(denseComponents)[denseIndex]);
    }

    template <std::size_t... Is>
    std::tuple<ComponentTypes*...> getEntryPtrTuple(IndexType denseIndex, std::index_sequence<Is...>) 
    {
        return std::make_tuple(getComponentPtrHelper<Is>(denseIndex)...);
    }

    template <std::size_t... Is>
    std::tuple<const ComponentTypes*...> getEntryPtrTupleConst(IndexType denseIndex, std::index_sequence<Is...>) const 
    {
        return std::make_tuple(getComponentPtrHelperConst<Is>(denseIndex)...);
    }
};


template<typename... ComponentTypes>
class View
{
public:
    using IterFunc = std::function<void(EntityID, ComponentTypes&...)>;

    View(const std::array<BaseSparseSet*, sizeof...(ComponentTypes)>& componentPools)
        : pools{ componentPools }, smallestPool{ nullptr }
    {
        size_t minSize = std::numeric_limits<size_t>::max();
        for (BaseSparseSet* pool : pools)
        {
            if (!pool) 
            {
                 throw std::runtime_error("Null pool provided to View constructor");
            }
            size_t currentSize = pool->getSize();
            if (currentSize < minSize)
            {
                smallestPool = pool;
                minSize = currentSize;
            }
        }        

        if constexpr (sizeof...(ComponentTypes) > 0) 
        {
             if (!smallestPool) 
             {
                 if (!pools.empty()) 
                 {
                    smallestPool = pools[0];
                 }
             }
        }
        else
        {
            throw std::runtime_error("You must provide at least one Component Type");
        }
    }

    void iterate(IterFunc func) const
    {
        if constexpr (sizeof...(ComponentTypes) == 0) return;
        if (!smallestPool || smallestPool->getSize() == 0) return;

        for (const EntityID entityID : smallestPool->getEntityIds())
        {
            bool hasAllComponents = true;
            for (const BaseSparseSet* pool : pools)
            {
                if (pool == smallestPool) continue;

                if (!pool->contains(entityID))
                {
                    hasAllComponents = false;
                    break;
                }
            }

            if (hasAllComponents)
            {
                invokeCallback(func, entityID, std::index_sequence_for<ComponentTypes...>{});
            }
        }
    }

private:
    std::array<BaseSparseSet*, sizeof...(ComponentTypes)> pools;
    BaseSparseSet* smallestPool;

    template <std::size_t I>
    auto& getComponentForEntity(EntityID entityID) const
    {
        using SpecificComponentType = std::tuple_element_t<I, std::tuple<ComponentTypes...>>;
        return static_cast<SparseSet<SpecificComponentType>*>(pools[I])->getEntry(entityID);
    }

    template <typename Func, std::size_t... Is>
    void invokeCallback(Func&& func, EntityID entityID, std::index_sequence<Is...>) const
    {
        func(entityID, getComponentForEntity<Is>(entityID)...);
    }
};

class ECS
{
template <typename... ComponentTypes>
friend struct GroupMigrator;
public:
    static constexpr size_t MAX_COMPONENT_TYPES = 64;
    static constexpr size_t MAX_ENTITY_COUNT = 67108864;
    using ComponentBitset = std::bitset<MAX_COMPONENT_TYPES>;

    ECS()
    {
        reusableIds.reserve(1024);
    }

    ~ECS() = default;

    ECS(const ECS&) = delete;
    ECS& operator=(const ECS&) = delete;
    ECS(ECS&&) = default; 
    ECS& operator=(ECS&&) = default;

    template<typename ComponentType>
	void registerComponent()
	{
		size_t index = TypeRegistry::getID<ComponentType>();

		if (index >= componentPools.size()) 
        {
			componentPools.resize(index + 1, nullptr);
            componentPoolBitsets.resize(index + 1);
		}

		if (componentPools[index]) 
        { // Check if the unique_ptr is non-null
			throw std::runtime_error("Component type already registered at this index.");
		}

		componentPools[index] = std::make_unique<SparseSet<ComponentType>>();
        componentPoolBitsets[index] = ComponentBitset(1ULL << index);
	}

	template<typename... ComponentTypes>
	void registerComponentGroup()
	{
		using SortedComponentTypes = TypeSorting::SortedTypes<ComponentTypes...>;
		
		size_t index = TypeRegistry::getID<SortedComponentTypes>();

		if (index >= groupPools.size()) 
		{
            groupPools.resize(index + 1, nullptr);
            groupPoolBitsets.resize(index + 1);
		}

		if (groupPools[index]) 
		{ 
			throw std::runtime_error("Component group already registered at this index.");
		}

        groupPools[index] = std::make_unique<MultiSparseSet<SortedComponentTypes>>();
        groupPoolBitsets[index] = ComponentBitset(0);
        migrators.emplace_back(std::make_unique<GroupMigrator<ComponentTypes...>>());

		([&]
        {
			size_t typeIndex = TypeRegistry::getID<ComponentTypes>();
			if (typeIndex >= componentPools.size()) 
			{
				componentPools.resize(typeIndex + 1, nullptr);
                componentPoolBitsets.resize(typeIndex + 1);
			}

            if (!componentPools[typeIndex])
            {
                componentPools[typeIndex] = std::make_unique<SparseSet<ComponentTypes>>();
                componentPoolBitsets[typeIndex] = ComponentBitset(1ULL << typeIndex);
            }

            // Not typeIndex as we are adding to the multi sparse set not the individual ones
            groupPoolBitsets[index].set(typeIndex, true);
        }(), ...); 
	}

    EntityID createEntity()
    {
        EntityID id;
        if (!reusableIds.empty())
        { 
            id = reusableIds.back();
            reusableIds.pop_back();
        }
        else
        {
            if (idCounter >= MAX_ENTITY_COUNT) throw std::runtime_error("ID TO BIG");
            id = idCounter++;
        }

        entityComponentMasks.addComponent(id, ComponentBitset(0ULL));

        return id;
    }

    void destroyEntity(EntityID id)
    {
        if (!entityComponentMasks.contains(id))
        {
            #ifdef DEBUG
            std::cerr << "Warning: Attempting to destroy non-existent or already destroyed entity ID " << id << std::endl;
            #endif
            return; 
        }

        const ComponentBitset mask = entityComponentMasks.getEntry(id);

        for (size_t componentId = 0; componentId < MAX_COMPONENT_TYPES; ++componentId)
        {
            if (mask.test(componentId))
            {
                if (componentId < componentPools.size() && componentPools[componentId])
                {
                    BaseSparseSet* pool = componentPools[componentId].get();

                    bool deleted = pool->deleteEntry(id);

                    #ifdef DEBUG
                    if (!deleted) 
                    {
                         std::cerr << "Warning: Inconsistency during destroyEntity(" << id
                                   << "). Mask indicated component " << componentId
                                   << " present, but deletion failed in its SparseSet." << std::endl;
                    }
                    #else
                    (void)deleted; 
                    #endif
                }
                #ifdef DEBUG
                else 
                {
                     std::cerr << "Warning: Inconsistency during destroyEntity(" << id
                               << "). Mask indicated component " << componentId
                               << " present, but the component pool is null or unregistered." << std::endl;
                }
                #endif
            }
        }

        entityComponentMasks.deleteEntry(id);

        reusableIds.push_back(id);
    }

    template<typename ComponentType>
    void addComponent(EntityID id, const ComponentType& component)
    {
		addComponentInternal(id, component);
    }

    template<typename ComponentType>
    void addComponent(EntityID id, ComponentType&& component)
    {
        addComponentInternal(id, std::move(component));
    }
    
    template<typename ComponentType>
    bool hasComponent(EntityID id)
    {
        if (!entityComponentMasks.contains(id))
        {
             #ifdef DEBUG
             std::cerr << "Warning: hasComponent called on non-existent or destroyed entity ID " << id << std::endl;
             #endif
            return false; 
        }

        size_t componentId = TypeRegistry::getID<ComponentType>();
        if (componentId >= componentPools.size() || !componentPools[componentId])
        {
             throw std::runtime_error("Component type with ID " + std::to_string(componentId) + " not registered (in hasComponent).");
        }

        const ComponentBitset& mask = entityComponentMasks.getEntry(id);
        return mask.test(componentId);   
    }


	template<typename ComponentType>
    ComponentType& getComponent(EntityID id) const
    {
        if (!entityComponentMasks.contains(id))
        {
            throw std::out_of_range("Entity ID " + std::to_string(id) + " does not exist (in getComponent).");
        }

        size_t componentId = TypeRegistry::getID<ComponentType>();
		if (componentId >= componentPools.size() || !componentPools[componentId])
        {
            throw std::runtime_error("Component type " + std::to_string(componentId) + " not registered or pool is null (in getComponent).");
		}

        const ComponentBitset& mask = entityComponentMasks.getEntry(id);
        if (!mask.test(componentId))
        {
             throw std::runtime_error("Entity ID " + std::to_string(id) + " does not have component " + std::to_string(componentId) + " according to its mask (in getComponent).");
        }

        SparseSet<ComponentType>* sparseSetPtr = static_cast<SparseSet<ComponentType>*>(componentPools[componentId].get());

        try 
        {
            return sparseSetPtr->getEntry(id);
        } 
        catch (const std::out_of_range& e) 
        {
             throw std::runtime_error("Component " + std::to_string(componentId) + " for entity " + std::to_string(id) +
                                      " not found in its individual SparseSet. It might be part of a Component Group. Use getComponentsFromGroup if applicable. Original error: " + e.what());
        }
    }

    template<typename... ComponentTypes>
	std::tuple<ComponentTypes&...> getComponentsFromGroup(EntityID id)
	{
        static_assert(sizeof...(ComponentTypes) > 0, "Must request at least one component type for a group.");

		using SortedGroupTypes = typename TypeSorting::SortedTypes<ComponentTypes...>;
		size_t groupIndex = TypeRegistry::getID<SortedGroupTypes>();

        if (groupIndex >= groupPools.size() || !groupPools[groupIndex])
		{
			throw std::runtime_error("Component group is not registered (in getComponentsFromGroup).");
		}

        if (!entityComponentMasks.contains(id))
        {
            throw std::out_of_range("Entity ID " + std::to_string(id) + " does not exist (in getComponentsFromGroup).");
        }

        const ComponentBitset& entityMask = entityComponentMasks.getEntry(id);
        const ComponentBitset& expectedGroupMask = groupPoolBitsets[groupIndex]; 

        if (entityMask != expectedGroupMask)
        {
             throw std::runtime_error("Entity ID " + std::to_string(id) + " has component mask (" + entityMask.to_string() +
                                      ") which does not exactly match the requested group mask (" + expectedGroupMask.to_string() +
                                      ") (in getComponentsFromGroup).");
        }

        auto* multiSet = static_cast<MultiSparseSet<SortedGroupTypes>*>(groupPools[groupIndex].get());

        auto sortedTuple = multiSet->getEntry(id); 

        return std::tie(std::get<ComponentTypes&>(sortedTuple)...);
	}

    template<typename ComponentType>
    void removeComponent(EntityID id)
    {
        if (!entityComponentMasks.contains(id)) 
        {
            #ifdef DEBUG
			throw std::runtime_error("Entity ID " + std::to_string(id) + " does not exist (in removeComponent).");
            #endif // DEBUG

            return; 
		}

        size_t componentId = TypeRegistry::getID<ComponentType>();

        if (componentId >= componentPools.size() || !componentPools[componentId]) 
        {
            #ifdef DEBUG
            throw std::runtime_error("Component type not registered (in removeComponent).");
            #endif

            return;
        }

        ComponentBitset& mask = entityComponentMasks.getEntry(id); 
        if (!mask.test(componentId)) 
        {
            return; 
        }

        SparseSet<ComponentType>* sparseSetPtr = static_cast<SparseSet<ComponentType>*>(componentPools[componentId].get());
        bool deleted = sparseSetPtr->deleteEntry(id); 

        if (deleted) 
        {
             mask.reset(componentId); 
        } 
        else 
        {
            throw std::logic_error("Inconsistency: Mask indicated component presence, but deletion failed for entity "
                                   + std::to_string(id) + " component " + std::to_string(componentId));
        }        
    }

    
    template<typename... ComponentTypes>
    View<ComponentTypes...> getView()
    {
        if constexpr (sizeof...(ComponentTypes) == 0) 
        {
            return View<ComponentTypes...>({}); // Pass empty array
        }

        std::array<BaseSparseSet*, sizeof...(ComponentTypes)> viewPools{}; 
        std::string error_message;

        auto get_and_check_pool = [&]<std::size_t I>() -> bool 
        {
            using ComponentType = std::tuple_element_t<I, std::tuple<ComponentTypes...>>;
            size_t componentId = TypeRegistry::getID<ComponentType>();

            if (componentId >= componentPools.size() || !componentPools[componentId]) 
            {
                 if (!error_message.empty()) error_message += ", ";
                 error_message += "Component type with ID " + std::to_string(componentId) + " not registered or pool is null";
                 return false; 
            }
            viewPools[I] = componentPools[componentId].get(); 
            return true; 
        };

        bool all_valid = [&]<std::size_t... Is>(std::index_sequence<Is...>) 
        {
             // The '&& ...' fold requires C++17
             return (get_and_check_pool.template operator()<Is>() && ...);
        }(std::index_sequence_for<ComponentTypes...>{});


        if (!all_valid) 
        {
            throw std::runtime_error("Cannot create view due to unregistered/invalid component types: " + error_message);
        }

        return View<ComponentTypes...>(viewPools);
    }

	class TypeSorting
    {
    private:
		template <typename T>
		constexpr std::string_view type_name() 
        {
			#if defined(__clang__) || defined(__GNUC__)
				return __PRETTY_FUNCTION__;
			#elif defined(_MSC_VER)
				return __FUNCSIG__;
			#else
				return typeid(T).name();
			#endif
		}


		constexpr int compare_strings(const char* a, const char* b) {
			while (*a && *a == *b) {
				++a; ++b;
			}
			if (*a < *b) return -1;
			if (*a > *b) return 1;
			return 0; 
		}


		template <typename... Ts>
		struct TypeList {};

		template <typename A, typename B>
		struct LessThan 
        {
			static constexpr std::string_view name_a = type_name<A>();
			static constexpr std::string_view name_b = type_name<B>();
			static constexpr bool value = name_a.compare(name_b) < 0;
		};

		template <typename T, typename TL> struct PushFrontT;
		template <typename T, template<typename...> class TL, typename... Ts>
		struct PushFrontT<T, TL<Ts...>> {
			using type = TL<T, Ts...>;
		};

		template <typename List1, typename List2>
		struct Merge;

		template <typename... Ts>
		struct Merge<TypeList<Ts...>, TypeList<>> {
			using type = TypeList<Ts...>;
		};

		template <typename... Ts>
		struct Merge<TypeList<>, TypeList<Ts...>> {
			using type = TypeList<Ts...>;
		};

		template <typename A, typename... As, typename B, typename... Bs>
		struct Merge<TypeList<A, As...>, TypeList<B, Bs...>> {
		private:
			using Tail = std::conditional_t<
				LessThan<A, B>::value,
				typename Merge<TypeList<As...>, TypeList<B, Bs...>>::type, // Keep B, Bs...
				typename Merge<TypeList<A, As...>, TypeList<Bs...>>::type  // Keep A, As...
			>;

			using Head = std::conditional_t<
				LessThan<A, B>::value,
				A,
				B
			>;

		public:
			using type = typename PushFrontT<Head, Tail>::type;
		};


		template <typename TL>
		struct MergeSort;

		template <>
		struct MergeSort<TypeList<>> {
			using type = TypeList<>;
		};

		template <typename T>
		struct MergeSort<TypeList<T>> {
			using type = TypeList<T>;
		};

		template <typename... Ts>
		struct MergeSort<TypeList<Ts...>> {
		private:
			static constexpr size_t size = sizeof...(Ts);
			static constexpr size_t mid = size / 2;

			template <typename IdxSeq, typename Tuple> struct Splitter;

			template <size_t... Is, typename... Types>
			struct Splitter<std::index_sequence<Is...>, std::tuple<Types...>> {
				using first_half = TypeList<std::tuple_element_t<Is, std::tuple<Types...>>...>;
			};

			template <typename IdxSeq, typename Tuple, size_t Offset> struct SplitterSecond;
			template <size_t... Is, typename... Types, size_t Offset>
			 struct SplitterSecond<std::index_sequence<Is...>, std::tuple<Types...>, Offset> {
				using second_half = TypeList<std::tuple_element_t<Is + Offset, std::tuple<Types...>>...>;
			};

			using TupleType = std::tuple<Ts...>;
			using FirstHalf = typename Splitter<std::make_index_sequence<mid>, TupleType>::first_half;
			using SecondHalf = typename SplitterSecond<std::make_index_sequence<size - mid>, TupleType, mid>::second_half;


			using SortedFirst = typename MergeSort<FirstHalf>::type;
			using SortedSecond = typename MergeSort<SecondHalf>::type;

		public:
			using type = typename Merge<SortedFirst, SortedSecond>::type;
		};

    public:
		template <typename... Ts>
		using SortedTypes = typename MergeSort<TypeList<Ts...>>::type;

    };

private:
    struct IGroupMigrator;
	template <typename NewComponentType>
	struct IMigratorVisitor;

	enum class MigrationType 
    {
		FromGroup,
		FromIndividual,
		ToIndividual
	};

	struct IGroupMigrator 
    {
		virtual ~IGroupMigrator() = default;
		
		// Base migration functions for different scenarios
		virtual void migrateFromGroup(ECS* ecs, EntityID id, const ComponentBitset& targetMask, const ComponentBitset& oldMask) = 0;
		virtual void migrateFromIndividual(ECS* ecs, EntityID id, const ComponentBitset& targetMask, const ComponentBitset& oldMask) = 0;
		virtual void migrateToIndividual(ECS* ecs, EntityID id, const ComponentBitset& targetMask, const ComponentBitset& oldMask) = 0;
		
		template <typename NewComponentType>
		void accept(IMigratorVisitor<NewComponentType>& visitor, ECS* ecs, EntityID id, 
					const ComponentBitset& targetMask, const ComponentBitset& oldMask, 
					NewComponentType&& newData, MigrationType migrationType) 
        {
			visitor.visit(*this, ecs, id, targetMask, oldMask, std::forward<NewComponentType>(newData), migrationType);
		}
	};

	template <typename... ComponentTypes>
	struct GroupMigrator : public IGroupMigrator 
    {
		// Migration from a group to another group
		void migrateFromGroup(ECS* ecs, EntityID id, const ComponentBitset& targetMask, const ComponentBitset& oldMask) override 
        {
			using SortedGroupTypes = typename TypeSorting::SortedTypes<ComponentTypes...>; 
			size_t groupIndex = TypeRegistry::getID<SortedGroupTypes>();

			if (groupIndex >= ecs->groupPools.size() || !ecs->groupPools[groupIndex]) {
				throw std::runtime_error("Target group pool for migration does not exist.");
			}

			MultiSparseSet<SortedGroupTypes>* multiSet = 
				static_cast<MultiSparseSet<SortedGroupTypes>*>(ecs->groupPools[groupIndex].get());

			// Get all components from the entity
			multiSet->addComponentMulti(id, ecs->getComponent<ComponentTypes>(id)...);

			// Update the component mask
			ComponentBitset& mask = ecs->entityComponentMasks.getEntry(id);
			mask = targetMask;
		}

		// Migration from individual components to a group
		void migrateFromIndividual(ECS* ecs, EntityID id, const ComponentBitset& targetMask, const ComponentBitset& oldMask) override 
        {
			using SortedGroupTypes = typename TypeSorting::SortedTypes<ComponentTypes...>; 
			size_t groupIndex = TypeRegistry::getID<SortedGroupTypes>();

			if (groupIndex >= ecs->groupPools.size() || !ecs->groupPools[groupIndex]) 
            {
				throw std::runtime_error("Target group pool for migration does not exist.");
			}

			MultiSparseSet<SortedGroupTypes>* multiSet = 
				static_cast<MultiSparseSet<SortedGroupTypes>*>(ecs->groupPools[groupIndex].get());

			// Get all components from individual sets and add to group
			multiSet->addComponentMulti(id, ecs->getComponent<ComponentTypes>(id)...);

			// Remove components from individual sets
			ComponentBitset& mask = ecs->entityComponentMasks.getEntry(id);
			([&] {
				size_t componentId = TypeRegistry::getID<ComponentTypes>();
				if (ecs->componentPools[componentId]) { 
					auto* singleSet = static_cast<SparseSet<ComponentTypes>*>(ecs->componentPools[componentId].get());
					if (singleSet->contains(id)) { 
						singleSet->deleteEntry(id);
					}
				}
			}(), ...);
			
			mask = targetMask;
		}

		// Migration from a group to individual components
		void migrateToIndividual(ECS* ecs, EntityID id, const ComponentBitset& targetMask, const ComponentBitset& oldMask) override 
        {
			// First get all components from the group
			using SortedGroupTypes = typename TypeSorting::SortedTypes<ComponentTypes...>; 
			size_t groupIndex = TypeRegistry::getID<SortedGroupTypes>();

			if (groupIndex >= ecs->groupPools.size() || !ecs->groupPools[groupIndex]) 
            {
				throw std::runtime_error("Source group pool for migration does not exist.");
			}

			MultiSparseSet<SortedGroupTypes>* multiSet = static_cast<MultiSparseSet<SortedGroupTypes>*>(ecs->groupPools[groupIndex].get());

			// Add components to individual sets
			([&] {
				auto& component = multiSet->template getComponent<ComponentTypes>(id);
				size_t componentId = TypeRegistry::getID<ComponentTypes>();
				
				if (componentId >= ecs->componentPools.size() || !ecs->componentPools[componentId]) 
                {
					ecs->componentPools[componentId] = std::make_unique<SparseSet<ComponentTypes>>();
				}
				
				auto* singleSet = static_cast<SparseSet<ComponentTypes>*>(ecs->componentPools[componentId].get());
				singleSet->addComponent(id, std::move(component));
			}(), ...);

			// Remove from group
			multiSet->deleteEntry(id);

			// Update the component mask
			ComponentBitset& mask = ecs->entityComponentMasks.getEntry(id);
			mask = targetMask;
		}

		// Migration with new component data
		template <typename NewComponentType>
		void migrate(ECS* ecs, EntityID id, const ComponentBitset& targetMask, 
					 const ComponentBitset& oldMask, NewComponentType&& newData, MigrationType migrationType) 
        {
			switch (migrationType) 
            {
			case MigrationType::FromGroup:
				migrateFromGroup(ecs, id, targetMask, oldMask);
				break;
			case MigrationType::FromIndividual:
				migrateFromIndividual(ecs, id, targetMask, oldMask);
				break;
			case MigrationType::ToIndividual:
				migrateToIndividual(ecs, id, targetMask, oldMask);
				break;
			}
		}
		
		template <typename NewComponentType>
		void accept(IMigratorVisitor<NewComponentType>& visitor, ECS* ecs, EntityID id, 
					const ComponentBitset& targetMask, const ComponentBitset& oldMask, 
					NewComponentType&& newData, MigrationType migrationType) {
			visitor.visit(*this, ecs, id, targetMask, oldMask, std::forward<NewComponentType>(newData), migrationType);
		}
	};

	template <typename NewComponentType>
	struct IMigratorVisitor {
		virtual ~IMigratorVisitor() = default;
		
		template <typename... ComponentTypes>
		void visit(GroupMigrator<ComponentTypes...>& migrator, ECS* ecs, EntityID id, 
				   const ComponentBitset& targetMask, const ComponentBitset& oldMask, 
				   NewComponentType&& newData, MigrationType migrationType) {
			migrator.migrate(ecs, id, targetMask, oldMask, std::forward<NewComponentType>(newData), migrationType);
		}
		
		virtual void visit(IGroupMigrator& migrator, ECS* ecs, EntityID id, 
						   const ComponentBitset& targetMask, const ComponentBitset& oldMask, 
						   NewComponentType&& newData, MigrationType migrationType) 
        {
			switch (migrationType) 
            {
				case MigrationType::FromGroup:
					migrator.migrateFromGroup(ecs, id, targetMask, oldMask);
					break;
				case MigrationType::FromIndividual:
					migrator.migrateFromIndividual(ecs, id, targetMask, oldMask);
					break;
				case MigrationType::ToIndividual:
					migrator.migrateToIndividual(ecs, id, targetMask, oldMask);
					break;
			}
		}
	};


	std::vector<EntityID> reusableIds;

    std::vector<std::unique_ptr<BaseSparseSet>> componentPools;
    std::vector<ComponentBitset> componentPoolBitsets;

    std::vector<std::unique_ptr<BaseSparseSet>> groupPools;
    std::vector<ComponentBitset> groupPoolBitsets;
    std::vector<std::unique_ptr<IGroupMigrator>> migrators;


    SparseSet<ComponentBitset> entityComponentMasks;
    
    EntityID idCounter = 0;

	template<typename ComponentType, typename ComponentArg>
    void addComponentInternal(EntityID id, ComponentArg&& component)
    {
         if (!entityComponentMasks.contains(id)) 
         {
             throw std::runtime_error("Entity ID " + std::to_string(id) + " does not exist (in addComponent).");
         }

         size_t componentId = TypeRegistry::getID<ComponentType>();

         if (componentId >= componentPools.size() || !componentPools[componentId]) 
         {
             throw std::runtime_error("Component type not registered (in addComponent).");
         }

         ComponentBitset& currentMask = entityComponentMasks.getEntry(id); 
         if (currentMask.test(componentId)) 
         {
             throw std::runtime_error("Entity ID " + std::to_string(id) + " already has component " + std::to_string(componentId) + " (checked via mask).");
         }

         ComponentBitset oldMask = currentMask;
         ComponentBitset newMask = oldMask;

         newMask.set(componentId, true);

         IndexType foundIndex = INVALID_INDEX;
         IndexType srcGroupIndex = INVALID_INDEX;
         for (IndexType i = 0; i < groupPoolBitsets.size(); ++i)
         {
			if (groupPoolBitsets[i] == newMask)
			{
                foundIndex = i;
			}
            if (groupPoolBitsets[i] == oldMask)
            {
                srcGroupIndex = i;
            }
         }

        switch ((foundIndex != INVALID_INDEX) << 1 | (srcGroupIndex != INVALID_INDEX))
		{
		case 0b11: // foundIndex != INVALID_INDEX && srcGroupIndex != INVALID_INDEX
		{
			IMigratorVisitor<ComponentType> visitor;
			auto& migrator = migrators[foundIndex];
			migrator->accept(visitor, this, id, newMask, oldMask, std::forward<ComponentArg>(component), MigrationType::FromGroup);
			break;
		}
		case 0b10: // foundIndex != INVALID_INDEX && srcGroupIndex == INVALID_INDEX
		{
			IMigratorVisitor<ComponentType> visitor;
			auto& migrator = migrators[foundIndex];
			migrator->accept(visitor, this, id, newMask, oldMask, std::forward<ComponentArg>(component), MigrationType::FromIndividual);
			break;
		}
		case 0b01: // foundIndex == INVALID_INDEX && srcGroupIndex != INVALID_INDEX
		{
			IMigratorVisitor<ComponentType> visitor;
			auto& migrator = migrators[srcGroupIndex];
			migrator->accept(visitor, this, id, newMask, oldMask, std::forward<ComponentArg>(component), MigrationType::ToIndividual);
			break;
		}
		case 0b00: // foundIndex == INVALID_INDEX && srcGroupIndex == INVALID_INDEX
		{
			SparseSet<ComponentType>* sparseSetPtr = static_cast<SparseSet<ComponentType>*>(componentPools[componentId].get());
			sparseSetPtr->addComponent(id, std::forward<ComponentArg>(component));
			currentMask = newMask;
			break;
		}
		}
         
    }

    
    class TypeRegistry 
    {
	public:
		template <typename T>
		static size_t getID() 
        {
			static const size_t singleTypeId = nextID++;
			return singleTypeId;
		}

        template<typename... Ts>
        static size_t getID()
        {
            static const size_t groupId = nextGroupID++;
            return groupId;
        }

	private:
		inline static size_t nextID = 0;
		inline static size_t nextGroupID = 0;
	};

};
