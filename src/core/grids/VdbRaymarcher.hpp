#ifndef VDBRAYMARCHER_HPP_
#define VDBRAYMARCHER_HPP_

#if OPENVDB_AVAILABLE

#include <openvdb/openvdb.h>
#include <openvdb/math/DDA.h>

namespace Tungsten {

class DdaRay
{
    openvdb::Vec3f _pos, _dir, _invDir;
public:
    typedef float RealType;
    typedef openvdb::Vec3f Vec3Type;
    DdaRay(Vec3f p, Vec3f w) : _pos(p.x(), p.y(), p.z()), _dir(w.x(), w.y(), w.z()), _invDir(1.0f/_dir) {}
    inline const openvdb::Vec3f &dir() const { return _dir; }
    inline const openvdb::Vec3f &invDir() const { return _invDir; }
    inline openvdb::Vec3f operator()(float t) const { return _pos + _dir*t; }
};

template <typename TreeT, int ChildNodeLevel>
class VdbRaymarcher
{
    typedef typename TreeT::RootNodeType::NodeChainType ChainT;
    typedef typename boost::mpl::at<ChainT, boost::mpl::int_<ChildNodeLevel - 1>>::type NodeT;
    typedef typename openvdb::tree::ValueAccessor<const TreeT> AccessorT;

    openvdb::math::DDA<DdaRay, NodeT::TOTAL> _dda;
    VdbRaymarcher<TreeT, ChildNodeLevel - 1> _child;

public:
    template<typename Intersector>
    bool march(const DdaRay &ray, float t0, float t1, AccessorT &acc, Intersector intersector)
    {
        _dda.init(ray, t0, t1);
        float ta = t0;
        do {
            openvdb::Coord voxel = _dda.voxel();
            _dda.step();
            float tb = min(_dda.time(), t1);

            if (acc.template probeConstNode<NodeT>(voxel) != NULL) {
                if (_child.march(ray, ta, tb, acc, intersector))
                    return true;
            } else if (acc.isValueOn(voxel)) {
                if (intersector(voxel, ta, tb))
                    return true;
            }
            ta = tb;
        } while (ta < t1);
        return false;
    }
};

template <typename TreeT>
class VdbRaymarcher<TreeT, 0>
{
    typedef typename openvdb::tree::ValueAccessor<const TreeT> AccessorT;

    openvdb::math::DDA<DdaRay, 0> _dda;

public:
    template<typename Intersector>
    bool march(const DdaRay &ray, float t0, float t1, AccessorT &acc, Intersector intersector)
    {
        _dda.init(ray, t0, t1);
        float ta = t0;
        do {
            openvdb::Coord voxel = _dda.voxel();
            _dda.step();
            float tb = min(_dda.time(), t1);

            if (acc.isValueOn(voxel))
                if (intersector(voxel, ta, tb))
                    return true;

            ta = tb;
        } while (ta < t1);
        return false;
    }
};

}

#endif

#endif /* VDBRAYMARCHER_HPP_ */
