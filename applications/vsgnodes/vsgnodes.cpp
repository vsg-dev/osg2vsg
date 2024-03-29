#include <vsg/core/ref_ptr.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/core/Object.h>
#include <vsg/core/Auxiliary.h>

#include <vsg/nodes/Group.h>
#include <vsg/nodes/QuadGroup.h>
#include <vsg/nodes/LOD.h>

#include <iostream>
#include <vector>
#include <chrono>

struct PrintVisitor : public vsg::Visitor
{
    using Visitor::apply;

    void apply(vsg::Object& object) override
    {
        std::cout<<"apply(vsg::Object& "<<&object<<")"<<std::endl;
        object.traverse(*this);
    }

    void apply(vsg::Node& node) override
    {
        std::cout<<"apply(vsg::Node& "<<&node<<")"<<std::endl;
        node.traverse(*this);
    }

    void apply(vsg::Group& group) override
    {
        std::cout<<"apply(vsg::Group& "<<&group<<") children.size() = "<<group.children.size()<<std::endl;
        group.traverse(*this);
    }

    void apply(vsg::QuadGroup& group) override
    {
        std::cout<<"apply(vsg::QuadGroup& "<<&group<<") children.size() ="<<group.children.size()<<std::endl;
        group.traverse(*this);
    }

    void apply(vsg::LOD& lod) override
    {
        std::cout<<"apply(vsg::LOD& "<<&lod<<") children.size() = "<<lod.children.size()<<std::endl;
        lod.traverse(*this);
    }
};

int main(int /*argc*/, char** /*argv*/)
{

    auto group = vsg::Group::create();

    // set up LOD
    auto lod = vsg::LOD::create();
    lod->addChild(vsg::LOD::Child{0.5, vsg::Node::create()});
    lod->addChild(vsg::LOD::Child{0.0, vsg::Node::create()});
    group->addChild(lod);

    PrintVisitor visitor;
    group->accept(visitor);


    return 0;
}
