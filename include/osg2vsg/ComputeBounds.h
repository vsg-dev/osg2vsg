#pragma once

#include <vsg/maths/box.h>

#include <osg2vsg/Export.h>


namespace vsg
{

    class ComputeBounds : public vsg::ConstVisitor
    {
    public:
        ComputeBounds() {}

        vsg::dbox bounds;

        using MatrixStack = std::vector<vsg::mat4>;
        MatrixStack matrixStack;

        void apply(const vsg::Node& node)
        {
            node.traverse(*this);
        }

        void apply(const vsg::Group& group)
        {

            if (auto transform = dynamic_cast<const vsg::MatrixTransform*>(&group); transform!=nullptr)
            {
                apply(*transform);
            }
            else if (auto geometry = dynamic_cast<const vsg::Geometry*>(&group); geometry!=nullptr)
            {
                apply(*geometry);
            }
            group.traverse(*this);
        }

        void apply(const vsg::MatrixTransform& transform)
        {
            std::cout<<"Transform matrix="<<transform._matrix->value()<<std::endl;

            matrixStack.push_back(*transform._matrix);

            transform.traverse(*this);

            matrixStack.pop_back();
        }

        void apply(const vsg::Geometry& geometry)
        {
            std::cout<<"Have Geometry"<<std::endl;

            if (!geometry._arrays.empty())
            {
                geometry._arrays[0]->accept(*this);
            }
        }

        void apply(const vsg::vec3Array& vertices)
        {
            if (matrixStack.empty())
            {
                for(auto vertex : vertices) bounds.add(vertex);
            }
            else
            {
                auto matrix = matrixStack.back();
                for(auto vertex : vertices) bounds.add(matrix * vertex);
            }
        }
    };


} // namespace vsg;
