#pragma once

#include <vsg/all.h>

#include <iostream>
#include <chrono>

#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgUtil/Optimizer>
#include <osg/Billboard>
#include <osg/MatrixTransform>

#include <osg2vsg/GraphicsNodes.h>
#include <osg2vsg/ShaderUtils.h>

namespace osg2vsg
{

    class SceneAnalysisVisitor : public osg::NodeVisitor
    {
    public:
        SceneAnalysisVisitor();

        using Geometries = std::vector<osg::ref_ptr<osg::Geometry>>;
        using StateGeometryMap = std::map<osg::ref_ptr<osg::StateSet>, Geometries>;
        using TransformGeometryMap = std::map<osg::Matrix, Geometries>;

        struct TransformStatePair
        {
            std::map<osg::Matrix, StateGeometryMap> matrixStateGeometryMap;
            std::map<osg::ref_ptr<osg::StateSet>, TransformGeometryMap> stateTransformMap;
        };

        using ProgramTransformStateMap = std::map<osg::ref_ptr<osg::StateSet>, TransformStatePair>;

        using StateStack = std::vector<osg::ref_ptr<osg::StateSet>>;
        using StateSets = std::set<StateStack>;
        using MatrixStack = std::vector<osg::Matrixd>;
        using StatePair = std::pair<osg::ref_ptr<osg::StateSet>, osg::ref_ptr<osg::StateSet>>;
        using StateMap = std::map<StateStack, StatePair>;

        struct UniqueStateSet
        {
            bool operator() ( const osg::ref_ptr<osg::StateSet>& lhs, const osg::ref_ptr<osg::StateSet>& rhs) const
            {
                if (!lhs) return true;
                if (!rhs) return false;
                return lhs->compare(*rhs)<0;
            }
        };

        using UniqueStats = std::set<osg::ref_ptr<osg::StateSet>, UniqueStateSet>;

        using Masks = std::pair<uint32_t, uint32_t>;
        using MasksTransformStateMap = std::map<Masks, TransformStatePair>;

        StateStack statestack;
        StateMap stateMap;
        MatrixStack matrixstack;
        UniqueStats uniqueStateSets;
        ProgramTransformStateMap programTransformStateMap;
        MasksTransformStateMap masksTransformStateMap;
        bool writeToFileProgramAndDataSetSets = false;
        ShaderCompiler shaderCompiler;

        osg::ref_ptr<osg::StateSet> uniqueState(osg::ref_ptr<osg::StateSet> stateset, bool programStateSet);

        StatePair computeStatePair(osg::StateSet* stateset);

        void apply(osg::Node& node);
        void apply(osg::Group& group);
        void apply(osg::Transform& transform);
        void apply(osg::Billboard& billboard);
        void apply(osg::Geometry& geometry);

        void pushStateSet(osg::StateSet& stateset);
        void popStateSet();

        void pushMatrix(const osg::Matrix& matrix);
        void popMatrix();

        void print();

        osg::ref_ptr<osg::Node> createStateGeometryGraphOSG(StateGeometryMap& stateGeometryMap);
        osg::ref_ptr<osg::Node> createTransformGeometryGraphOSG(TransformGeometryMap& transformGeometryMap);
        osg::ref_ptr<osg::Node> createOSG();

        vsg::ref_ptr<vsg::Node> createStateGeometryGraphVSG(StateGeometryMap& stateGeometryMap, vsg::Paths& searchPaths, uint32_t requiredGeomAttributesMask);
        vsg::ref_ptr<vsg::Node> createTransformGeometryGraphVSG(TransformGeometryMap& transformGeometryMap, vsg::Paths& searchPaths, uint32_t requiredGeomAttributesMask);
        vsg::ref_ptr<vsg::Node> createVSG(vsg::Paths& searchPaths);

        vsg::ref_ptr<vsg::Node> createNewVSG(vsg::Paths& searchPaths);
    };
}
