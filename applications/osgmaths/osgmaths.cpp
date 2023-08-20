#include <osg/Group>
#include <osg/NodeVisitor>
#include <osg/ArgumentParser>
#include <osg/io_utils>

#include <osgDB/ReadFile>
#include <osgDB/WriteFile>

#include <vsg/all.h>

#include <iostream>
#include <vector>
#include <chrono>
#include <memory>

template<class T>
typename T::value_type difference(const T& lhs, const T& rhs)
{
    typename T::value_type delta = 0.0;
    for(std::size_t c=0; c<4; ++c)
        {
            for(std::size_t r=0; r<4; ++r)
            {
                delta += std::abs(lhs(c,r) - rhs(c,r));
            }
        }
    return delta;
}

inline vsg::dmat4 convert(const osg::Matrixd& m)
{
    return vsg::dmat4(m(0, 0), m(0, 1), m(0, 2), m(0, 3),
                      m(1, 0), m(1, 1), m(1, 2), m(1, 3),
                      m(2, 0), m(2, 1), m(2, 2), m(2, 3),
                      m(3, 0), m(3, 1), m(3, 2), m(3, 3));
}

inline osg::Matrixd convert(const vsg::dmat4& m)
{
    return osg::Matrixd(m(0, 0), m(0, 1), m(0, 2), m(0, 3),
                        m(1, 0), m(1, 1), m(1, 2), m(1, 3),
                        m(2, 0), m(2, 1), m(2, 2), m(2, 3),
                        m(3, 0), m(3, 1), m(3, 2), m(3, 3));
}

int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);

    uint32_t numTests = arguments.value(1000000, "-n");
    auto vsg_test = arguments.value(1, "--vsg");
    auto osg_test = arguments.value(1, "--osg");
    bool print_test = arguments.read("--print");

    std::vector<vsg::dmat4> vsg_dmatrices;
    std::vector<vsg::dmat4> vsg_transposed_dmatrices;
    std::vector<osg::Matrixd> osg_dmatrices;
    std::vector<osg::Matrixd> osg_transposed_dmatrices;

    vsg::dvec3 axis = vsg::normalize(vsg::dvec3(0.2, 0.4, 0.8));
    vsg::dvec3 direction(10000.0, 20000.0, 30000.0);

    for(uint32_t i = 0; i<numTests; ++i)
    {
        double ratio = double(i)/double(numTests-1);
        double angle = ratio * vsg::PI * 2.0;

        vsg::dmat4 rot = vsg::rotate(vsg::radians(angle), axis);
        vsg::dmat4 trans = vsg::translate(direction * ratio);
        vsg::dmat4 scale = vsg::scale(1.1+std::sin(angle), 1.1+std::cos(angle), 1.1+std::tan(angle));
        vsg::dmat4 mult = rot * scale * trans;
        vsg::dmat4 transposed = vsg::transpose(mult);

        vsg_dmatrices.emplace_back(mult);
        vsg_transposed_dmatrices.emplace_back(transposed);

        osg_dmatrices.emplace_back(convert(mult));
        osg_transposed_dmatrices.emplace_back(convert(transposed));
    }


    if (osg_test)
    {
        osg::Matrixd osg_identity;

        double osg_delta = 0.0;
        double osg_transposed_delta = 0.0;

        auto before_osg = vsg::clock::now();
        decltype(before_osg) mid_osg;

        if (print_test)
        {
            for(auto& matrix : osg_dmatrices)
            {
                auto inv = osg::Matrixd::inverse(matrix);
                auto result = difference(matrix * inv, osg_identity);
                osg_delta += result;

                std::cout<<"\n\nosgmatrix = "<<matrix<<std::endl;
                std::cout<<"inv = "<<inv<<std::endl;
                std::cout<<"mult = "<<(matrix * inv)<<std::endl;
                std::cout<<"result = "<<result<<std::endl;
            }

            mid_osg = vsg::clock::now();

            for(auto& matrix : osg_transposed_dmatrices)
            {
                auto inv = osg::Matrixd::inverse(matrix);
                auto result = difference(matrix * inv, osg_identity);
                osg_transposed_delta += result;

                std::cout<<"\n\nosgmatrix = "<<matrix<<std::endl;
                std::cout<<"inv = "<<inv<<std::endl;
                std::cout<<"mult = "<<(matrix * inv)<<std::endl;
                std::cout<<"result = "<<result<<std::endl;
            }
        }
        else
        {
            for(auto& matrix : osg_dmatrices)
            {
                osg::Matrixd::inverse(matrix);
            }

            mid_osg = vsg::clock::now();

            for(auto& matrix : osg_transposed_dmatrices)
            {
                osg::Matrixd::inverse(matrix);
            }
        }

        auto after_osg = vsg::clock::now();

        std::cout<<"Time for OSG operations "<<std::chrono::duration<double, std::chrono::milliseconds::period>(mid_osg - before_osg).count()<<"ms delta = "<<osg_delta<<std::endl;;
        std::cout<<"Time for OSG transposed operations "<<std::chrono::duration<double, std::chrono::milliseconds::period>(after_osg - mid_osg).count()<<"ms delta = "<<osg_transposed_delta<<std::endl;;
    }

    if (vsg_test)
    {
        vsg::dmat4 identity;

        double vsg_delta = 0.0;
        double vsg_transposed_delta = 0.0;


        auto before_vsg = vsg::clock::now();
        decltype(before_vsg) mid_vsg;

        if (print_test)
        {
            for(auto& matrix : vsg_dmatrices)
            {
                auto inv = vsg::inverse(matrix);
                auto result = difference(matrix * inv, identity);
                vsg_delta += result;
                std::cout<<"\n\nvsgmatrix = "<<matrix<<std::endl;
                std::cout<<"inv = "<<inv<<std::endl;
                std::cout<<"mult = "<<(matrix * inv)<<std::endl;
                std::cout<<"result = "<<result<<std::endl;
            }

            mid_vsg = vsg::clock::now();

            for(auto& matrix : vsg_transposed_dmatrices)
            {
                auto inv = vsg::inverse(matrix);
                auto result = difference(matrix * inv, identity);
                vsg_transposed_delta += result;
                std::cout<<"\n\nvsgmatrix = "<<matrix<<std::endl;
                std::cout<<"inv = "<<inv<<std::endl;
                std::cout<<"mult = "<<(matrix * inv)<<std::endl;
                std::cout<<"result = "<<result<<std::endl;
            }
        }
        else
        {
            for(auto& matrix : vsg_dmatrices)
            {
                vsg::inverse(matrix);
            }

            mid_vsg = vsg::clock::now();

            for(auto& matrix : vsg_transposed_dmatrices)
            {
                vsg::inverse(matrix);
            }
        }

        auto after_vsg = vsg::clock::now();


        std::cout<<"Time for VSG operations "<<std::chrono::duration<double, std::chrono::milliseconds::period>(mid_vsg - before_vsg).count()<<"ms delta = "<<vsg_delta<<std::endl;;
        std::cout<<"Time for VSG transposed operations "<<std::chrono::duration<double, std::chrono::milliseconds::period>(after_vsg - mid_vsg).count()<<"ms delta = "<<vsg_transposed_delta<<std::endl;;
    }

    return 0;
}
