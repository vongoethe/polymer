#include "math-core.hpp"

#include "polymer-typeid.hpp"

#include "cereal/cereal.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/polymorphic.hpp"
#include "cereal/types/base_class.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/access.hpp"

#include <unordered_map>

using namespace polymer;

 //////////////////
 //   Entities   //
 //////////////////

 // An Entity is an uniquely identifiable object in the Polymer runtime.
 using entity = uint64_t;
 constexpr entity kInvalidEntity = 0;

 // Provide a consistent way to retrieve an Entity to which a component belongs. 
 class component 
 {
     entity e;
 public:
     explicit component(entity e = kInvalidEntity) : e(e) {}
     entity get_entity() const { return e; }
 };

 // Hash functor for Components so they can be used in unordered containers. It uses the Component's Entity as the key value.
 struct component_hash 
 {
     entity operator()(const component & c) const { return c.get_entity(); }
 };

 // Systems are responsible for storing the component data instances associated with Entities.
 // They also perform all the logic for manipulating and processing their Components.
 // This base class provides an API for an entity_manager to associate Components with Entities in a data-driven manner.
 struct entity_manager;

 struct base_system : public non_copyable
 {
     entity_manager * factory;

     explicit base_system(entity_manager * f) : factory(f) {}
     virtual ~base_system() {}

     // Associates component with the Entity using serialized data. The void pointer 
     // and hash type is to subvert the need for a heavily templated component system. 
     virtual bool create(entity e, poly_typeid hash, void * data) = 0;

     // Destroys all of an entity's associated components
     virtual void destroy(entity e) = 0;

     // Helper function to signal to the entity manager that this system operates on these types of components
     template <typename S>
     void register_system_for_type(S * system, poly_typeid type) { register_system_for_type(get_typeid<S>(), type); }
     void register_system_for_type(poly_typeid system_type, poly_typeid type);
 };

 struct entity_manager
 {
     std::mutex createMutex;

     std::unordered_map<poly_typeid, poly_typeid> system_type_map;
     entity entity_counter { 0 }; // Autoincrementing value to generate unique ids.

     template <typename T, typename... Args>
     T * create_system(Args &&... args);

     void register_system_for_type(poly_typeid system_type, poly_hash_value def_type) 
     {
         system_type_map[def_type] = system_type;
     }

     entity create()
     {
         std::lock_guard<std::mutex> guard(createMutex);
         const entity e = ++entity_counter;
         return e;
     }

     void add_system(poly_typeid system_type, base_system * system)
     {
         if (!system) return;
         auto itr = systems.find(system_type);
         if (itr == systems.end()) systems.emplace(system_type, system);
     }

     std::unordered_map<poly_typeid, base_system *> systems;
 };

 template <typename T, typename... Args>
 T * entity_manager::create_system(Args &&... args) 
 {
     T * ptr = new T(std::forward<Args>(args)...);
     add_system(get_typeid<T>(), ptr);
     return ptr;
 }

 void base_system::register_system_for_type(poly_typeid system_type, poly_typeid type) { factory->register_system_for_type(system_type, type); }

///////////////////
// Serialization //
///////////////////

struct physics_component : public component
{
    physics_component() {};
    physics_component(entity e) : component(e) {}
    float value1, value2, value3;
};
POLYMER_SETUP_TYPEID(physics_component);

struct render_component : public component
{
    render_component() {}
    render_component(entity e) : component(e) {}
    float value1, value2, value3;
};
POLYMER_SETUP_TYPEID(render_component);

template <typename T>
std::string serialize_to_json(T e)
{
    std::ostringstream oss;
    {
        cereal::JSONOutputArchive json_archiver(oss);
        json_archiver(e);
    }
    return oss.str();
}

template <typename T>
void deserialize_from_json(const std::string & json_str, T & e)
{
    std::istringstream iss(json_str);
    cereal::JSONInputArchive json_archiver(iss);
    json_archiver(e);
}

template<class F> void visit_fields(render_component & o, F f)
{
    f("v1", o.value1);
    f("v2", o.value2);
    f("v3", o.value3);
}

template<class F> void visit_fields(physics_component & o, F f)
{
    f("v1", o.value1);
    f("v2", o.value2);
    f("v3", o.value3);
}

template<class Archive> void serialize(Archive & archive, render_component & m)
{
    visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) { archive(cereal::make_nvp(name, field)); });
}

template<class Archive> void serialize(Archive & archive, physics_component & m)
{
    visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) { archive(cereal::make_nvp(name, field)); });
}

struct ex_system_one final : public base_system
{
    const poly_typeid c1 = get_typeid<physics_component>();
    ex_system_one(entity_manager * f) : base_system(f) { register_system_for_type(this, c1); }
    ~ex_system_one() override { }
    bool create(entity e, poly_typeid hash, void * data) override final
    { 
        if (hash != c1) { return false; } 
        auto new_component = physics_component(e);
        new_component = *static_cast<physics_component *>(data);
        components[e] = std::move(new_component);
        return true;
    }
    void destroy(entity entity) override final { }
    std::unordered_map<entity, physics_component> components;
};
POLYMER_SETUP_TYPEID(ex_system_one);

struct ex_system_two final : public base_system
{
    const poly_typeid c2 = get_typeid<render_component>();
    ex_system_two(entity_manager * f) : base_system(f) { register_system_for_type(this, c2); }
    ~ex_system_two() override { }
    bool create(entity e, poly_typeid hash, void * data) override final
    { 
        if (hash != c2) { return false; } 
        auto new_component = render_component(e);
        new_component = *static_cast<render_component *>(data);
        components[e] = std::move(new_component);
        return true; 
    }
    void destroy(entity entity) override final { }
    std::unordered_map<entity, render_component> components;
};
POLYMER_SETUP_TYPEID(ex_system_two);

template<class F> void visit_systems(base_system * s, F f)
{
    f("system_one", dynamic_cast<ex_system_one *>(s));
    f("system_two", dynamic_cast<ex_system_two *>(s));
}

//////////////////////////
//   Transform System   //
//////////////////////////

struct scene_graph_component : public component
{
    scene_graph_component() {};
    scene_graph_component(entity e) : component(e) {}
    polymer::Pose local_pose;
    polymer::float3 local_scale;
    entity parent{ kInvalidEntity };
    std::vector<entity> children;
}; POLYMER_SETUP_TYPEID(scene_graph_component);

struct world_transform_component : public component
{
    world_transform_component() {};
    world_transform_component(entity e) : component(e) {}
    polymer::Pose world_pose;
}; POLYMER_SETUP_TYPEID(world_transform_component);

class transform_system final : public base_system
{
    // Used by other systems to group types of transforms (collision, interactables, renderable, etc)
    using transform_flags = uint16_t;

    std::unordered_map<entity, scene_graph_component> scene_graph_transforms; // todo - component pool
    std::unordered_map<entity, world_transform_component> world_transforms; // todo - component pool

    void recalculate_world_transform(entity child)
    {
        auto & node = scene_graph_transforms[child];
        auto & world_xform = world_transforms[child];

        // If the node has a parent then we can compute a new world transform,.
        if (node.parent != kInvalidEntity)
        {
            auto & parent_node = scene_graph_transforms[node.parent];
            world_xform.world_pose = node.local_pose * parent_node.local_pose;
        }
        else
        {
            // If the node has no parent, it should be considered already in world space.
            world_xform.world_pose = node.local_pose;
        }

        // For each child, calculate its new world transform
        for (auto & c : node.children) recalculate_world_transform(c);
    }

    void destroy_recursive(entity child)
    {
        auto & node = scene_graph_transforms[child];
        for (auto & n : node.children) destroy_recursive(n);

        // Erase world transform
        world_transforms.erase(child);

        // Erase itself after all children are gone
        scene_graph_transforms.erase(child);
    }

public:

    transform_system(entity_manager * f) : base_system(f) 
    { 
        register_system_for_type(this, get_typeid<scene_graph_component>()); 
        register_system_for_type(this, get_typeid<world_transform_component>());
    }

    ~transform_system() override { }

    bool create(entity e, poly_typeid hash, void * data) override final { return true; }

    bool create(entity e, const polymer::Pose local_pose, const float3 local_scale)
    {
        // todo - check already added

        scene_graph_transforms[e] = scene_graph_component(e);
        scene_graph_transforms[e].local_pose = local_pose;
        scene_graph_transforms[e].local_scale = local_scale;

        world_transforms[e] = world_transform_component(e);
        recalculate_world_transform(e);

        return true;
    }

    bool has_transform(entity e) const
    {
        const auto itr = scene_graph_transforms.find(e);
        if (itr != scene_graph_transforms.end()) return true;
        return false;
    }

    bool add_child(entity parent, entity child)
    {
        if (parent == kInvalidEntity) throw std::invalid_argument("parent was invalid");
        if (child == kInvalidEntity) throw std::invalid_argument("child was invalid");
        if (!has_transform(parent)) throw std::invalid_argument("parent has no transform component");
        if (!has_transform(child)) throw std::invalid_argument("child has no transform component");

        scene_graph_transforms[parent].children.push_back(child);
        scene_graph_transforms[child].parent = parent;
        recalculate_world_transform(parent);
        return true;
    }

    scene_graph_component * get_local_transform(entity e)
    {
        if (e == kInvalidEntity) return nullptr;
        auto itr = scene_graph_transforms.find(e);
        if (itr != scene_graph_transforms.end()) return &itr->second;
        else return nullptr;
    }

    world_transform_component * get_world_transform(entity e)
    {
        if (e == kInvalidEntity) return nullptr;
        auto itr = world_transforms.find(e);
        if (itr != world_transforms.end()) return &itr->second;
        else return nullptr;
    }

    entity get_parent(entity child) const
    {
        if (child == kInvalidEntity) return kInvalidEntity;
        const auto itr = scene_graph_transforms.find(child);
        if (itr != scene_graph_transforms.end())
        {
            if (itr->second.parent != kInvalidEntity) return itr->second.parent;
            else return kInvalidEntity;
        }
        return kInvalidEntity;
    }

    void remove_parent(entity child)
    {
        auto & child_node = scene_graph_transforms[child];
        if (child_node.parent != kInvalidEntity)
        {
            auto & parent_node = scene_graph_transforms[child_node.parent];
            parent_node.children.erase(std::remove(parent_node.children.begin(), parent_node.children.end(), child), parent_node.children.end());
            child_node.parent = kInvalidEntity;
            recalculate_world_transform(child);
        }
    }

    void destroy(entity e) override final 
    { 
        if (e == kInvalidEntity) throw std::invalid_argument("parent was invalid");
        if (!has_transform(e)) throw std::invalid_argument("no component exists for this entity");
        destroy_recursive(e);
    }
};

POLYMER_SETUP_TYPEID(transform_system);

IMPLEMENT_MAIN(int argc, char * argv[])
{
    entity_manager factory;

    auto xform_system = factory.create_system<transform_system>(&factory);

    auto root = factory.create();
    auto child1 = factory.create();
    auto child2 = factory.create(); 
    xform_system->create(root, Pose(make_rotation_quat_axis_angle({ 0, 1, 0 }, POLYMER_PI / 2.0), float3(0, 5.f, 0)), float3(1, 1, 1));
    xform_system->create(child1, Pose(make_rotation_quat_axis_angle({ 0, 1, 0 }, -(POLYMER_PI / 2.0)), float3(0, 0, 3.f)), float3(1, 1, 1));
    xform_system->create(child2, Pose(float4(0, 0, 0, 1), float3(4.f, 0, 0)), float3(1, 1, 1));

    xform_system->add_child(root, child1);
    xform_system->add_child(root, child2);

    std::cout << "Root " << xform_system->get_world_transform(root)->world_pose << std::endl;
    std::cout << "First child " << xform_system->get_world_transform(child1)->world_pose << std::endl;
    std::cout << "Second child" << xform_system->get_world_transform(child2)->world_pose << std::endl;

    std::cout << "Parent of root is " << xform_system->get_parent(root) << std::endl;
    std::cout << "Parent of first child is " << xform_system->get_parent(child1) << std::endl;
    std::cout << "Parent of second child is " << xform_system->get_parent(child2) << std::endl;

    xform_system->remove_parent(child1);
    std::cout << "Parent of first child was removed. New parent is: " << xform_system->get_parent(child1) << std::endl;
    std::cout << "first child / new transform: " << xform_system->get_world_transform(child1)->world_pose << std::endl;

    xform_system->destroy(child1);

    std::cout << "Destroyed first child should be nullptr: " << xform_system->get_local_transform(child1) << std::endl;

    // 32768

    uniform_random_gen gen;

    // debug ~4.6 seconds
    // release ~80 ms
    {
        scoped_timer t("create 16384 entities with 4 children each (65535 total)");
        for (int i = 0; i < 16384; ++i)
        {
            auto rootEntity = factory.create();
            xform_system->create(rootEntity,
                Pose(make_rotation_quat_axis_angle({ gen.random_float(), gen.random_float(), gen.random_float() }, POLYMER_PI),
                    float3(gen.random_float() * 10, gen.random_float() * 10, gen.random_float() * 10)), float3(1, 1, 1));

            for (int c = 0; c < 4; ++c)
            {
                auto childEntity = factory.create();
                xform_system->create(childEntity,
                    Pose(make_rotation_quat_axis_angle({ gen.random_float(), gen.random_float(), gen.random_float() }, POLYMER_PI),
                        float3(gen.random_float() * 10, gen.random_float() * 10, gen.random_float() * 10)), float3(1, 1, 1));
                xform_system->add_child(rootEntity, childEntity);
            }
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(100));

    return EXIT_SUCCESS;
}

/*
IMPLEMENT_MAIN(int argc, char * argv[])
{
    entity_manager factory;

    render_component s(factory.create());
    s.value1 = 1.f;
    s.value2 = 2.f;
    s.value3 = 3.f;
    auto str = serialize_to_json(s);

    std::cout << "Render component attached entity: " << s.get_entity() << std::endl;

    render_component ds;
    deserialize_from_json(str, ds);

    std::cout << "Deserialized attached entity: " << ds.get_entity() << std::endl;

    auto sys1 = factory.create_system<ex_system_one>(&factory);
    auto sys2 = factory.create_system<ex_system_two>(&factory);

    auto someEntity = factory.create();
    std::cout << "New Entity is: " << someEntity << std::endl;

    sys2->create(someEntity, get_typeid<render_component>(), &ds);

    for (auto & e : sys2->components)
    {
        std::cout << "Iterate Entity: " << e.first << std::endl;
        visit_fields(e.second, [&](const char * name, auto & field, auto... metadata)
        {
            std::cout << "\tName: " << name << ", " << field << std::endl;
        });
    }

    // Hierarchy aware: transform, collision, events

    // Get list of all systems
    for (auto & system : factory.systems)
    {

        if (auto * sys = dynamic_cast<ex_system_one *>(system.second))
        {

        }
    }

    // Serialize an entity
    auto serialize_entity = [&](entity e)
    {

    };

    std::this_thread::sleep_for(std::chrono::seconds(100));

    return EXIT_SUCCESS;
}
*/