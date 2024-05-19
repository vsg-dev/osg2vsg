#pragma once

#include <vsg/all.h>

#include <osg/ArgumentParser>
#include <osg/PagedLOD>
#include <osgDB/ReadFile>
#include <osgTerrain/TerrainTile>
#include <osgUtil/MeshOptimizers>
#include <osgUtil/Optimizer>

#include "GeometryUtils.h"
#include "Optimize.h"
#include "SceneBuilder.h"
#include "ShaderUtils.h"

namespace osg2vsg
{

    using FileNameMap = std::map<std::string, vsg::Path>;

    class ConvertToVsg : public osg::NodeVisitor, public osg2vsg::SceneBuilderBase
    {
    public:
        ConvertToVsg(vsg::ref_ptr<const BuildOptions> options, vsg::ref_ptr<vsg::StateGroup> in_inheritedStateGroup = {}) :
            osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ACTIVE_CHILDREN),
            SceneBuilderBase(options),
            inheritedStateGroup(in_inheritedStateGroup)
        {
        }

        vsg::ref_ptr<vsg::Node> root;

        using osg::NodeVisitor::apply;
        using MasksAndState = std::tuple<uint32_t, uint32_t, osg::ref_ptr<osg::StateSet>>;
        using BindDescriptorSetMap = std::map<MasksAndState, vsg::ref_ptr<vsg::BindDescriptorSet>>;
        BindDescriptorSetMap bindDescriptorSetMap;
        vsg::ref_ptr<vsg::StateGroup> inheritedStateGroup;

        using NodeMap = std::map<osg::Node*, vsg::ref_ptr<vsg::Node>>;
        NodeMap nodeMap;

        size_t numOfPagedLOD = 0;
        FileNameMap filenameMap;

        vsg::ref_ptr<vsg::BindGraphicsPipeline> getOrCreateBindGraphicsPipeline(uint32_t shaderModeMask, uint32_t geometryMask);

        vsg::ref_ptr<vsg::BindDescriptorSet> getOrCreateBindDescriptorSet(uint32_t shaderModeMask, uint32_t geometryMask, osg::StateSet* stateset);

        vsg::Path mapFileName(const std::string& filename);

        void optimize(osg::Node* osg_scene);

        vsg::ref_ptr<vsg::Node> convert(osg::Node* node);

        template<class V>
        vsg::ref_ptr<V> copyArray(const osg::Array* array)
        {
            vsg::ref_ptr<V> new_array = V::create(array->getNumElements());

            std::memcpy(new_array->dataPointer(), array->getDataPointer(), array->getTotalDataSize());

            return new_array;
        }

        vsg::ref_ptr<vsg::Data> copy(osg::Array* src_array);

        struct ScopedPushPop
        {
            ConvertToVsg& convertToVsg;
            osg::StateSet* stateset;

            ScopedPushPop(ConvertToVsg& conv, osg::StateSet* ss) :
                convertToVsg(conv),
                stateset(ss)
            {
                if (stateset) convertToVsg.statestack.push_back(stateset);
            }

            ~ScopedPushPop()
            {
                if (stateset) convertToVsg.statestack.pop_back();
            }
        };

        uint32_t calculateShaderModeMask();

        void apply(osg::Geometry& geometry);
        void apply(osg::Group& group);
        void apply(osg::MatrixTransform& transform);
        void apply(osg::CoordinateSystemNode& cs);
        void apply(osg::Billboard& billboard);
        void apply(osg::LOD& lod);
        void apply(osg::PagedLOD& plod);
        void apply(osg::Switch& sw);
        void apply(osgTerrain::TerrainTile& terrainTile);
    };

} // namespace osg2vsg
