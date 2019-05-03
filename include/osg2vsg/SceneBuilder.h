#pragma once

#include <vsg/all.h>

#include <iostream>
#include <chrono>

#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgUtil/Optimizer>
#include <osg/Billboard>
#include <osg/MatrixTransform>

#include <osg2vsg/ShaderUtils.h>
#include <osg2vsg/GeometryUtils.h>

namespace osg2vsg
{

    class SceneBuilder : public osg::NodeVisitor
    {
    public:
        SceneBuilder();

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

        using GeometriesMap = std::map<const osg::Geometry*, vsg::ref_ptr<vsg::Command>>;
        using TexturesMap = std::map<const osg::Texture*, vsg::ref_ptr<vsg::Texture>>;

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
        GeometriesMap geometriesMap;
        TexturesMap texturesMap;
        bool writeToFileProgramAndDataSetSets = false;
        ShaderCompiler shaderCompiler;

        bool insertCullGroups = true;
        bool insertCullNodes = true;
        bool useBindDescriptorSet = true;

        GeometryTarget geometryTarget = VSG_VERTEXINDEXDRAW;

        uint32_t supportedGeometryAttributes = GeometryAttributes::ALL_ATTS;
        uint32_t supportedShaderModeMask = ShaderModeMask::ALL_SHADER_MODE_MASK;
        uint32_t overrideGeomAttributes = 0;
        uint32_t overrideShaderModeMask = ShaderModeMask::NONE;

        uint32_t nodeShaderModeMasks = ShaderModeMask::NONE;

        std::string vertexShaderPath = "";
        std::string fragmentShaderPath = "";

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

        vsg::ref_ptr<vsg::Node> createTransformGeometryGraphVSG(TransformGeometryMap& transformGeometryMap, vsg::Paths& searchPaths, uint32_t requiredGeomAttributesMask);

        vsg::ref_ptr<vsg::Node> createVSG(vsg::Paths& searchPaths);

        // core VSG style usage
        vsg::ref_ptr<vsg::Texture> convertToVsgTexture(const osg::Texture* osgtexture);
        vsg::ref_ptr<vsg::DescriptorSet> createVsgStateSet(const vsg::DescriptorSetLayouts& descriptorSetLayouts, const osg::StateSet* stateset, uint32_t shaderModeMask);
        vsg::ref_ptr<vsg::BindGraphicsPipeline> createBindGraphicsPipeline(uint32_t shaderModeMask, uint32_t geometryAttributesMask, const std::string& vertShaderPath = "", const std::string& fragShaderPath = "");
    };
}
