#ifndef BSDFLOBES_HPP_
#define BSDFLOBES_HPP_

#include "IntTypes.hpp"

namespace Tungsten
{

class BsdfLobes
{
    uint32 _lobes;

public:
    enum Lobe
    {
        GlossyReflectionLobe     = (1 << 0),
        GlossyTransmissionLobe   = (1 << 1),
        DiffuseReflectionLobe    = (1 << 2),
        DiffuseTransmissionLobe  = (1 << 3),
        SpecularReflectionLobe   = (1 << 4),
        SpecularTransmissionLobe = (1 << 5),
        AnisotropicLobe          = (1 << 6),

        GlossyLobe   =   GlossyReflectionLobe |   GlossyTransmissionLobe,
        DiffuseLobe  =  DiffuseReflectionLobe |  DiffuseTransmissionLobe,
        SpecularLobe = SpecularReflectionLobe | SpecularTransmissionLobe,

        TransmissiveLobe = GlossyTransmissionLobe | DiffuseTransmissionLobe | SpecularTransmissionLobe,
        ReflectiveLobe   = GlossyReflectionLobe   | DiffuseReflectionLobe   | SpecularReflectionLobe,

        AllLobes = TransmissiveLobe | ReflectiveLobe,

        InvalidLobe = 0
    };

    BsdfLobes()
    : _lobes(InvalidLobe)
    {
    }

    BsdfLobes(Lobe lobes)
    : _lobes(lobes)
    {
    }

    explicit BsdfLobes(uint32 lobes)
    : _lobes(lobes)
    {
    }

    BsdfLobes(const BsdfLobes &a, const BsdfLobes &b)
    : _lobes(0)
    {
        if (a.isGlossy() || b.isGlossy())
            _lobes |= GlossyLobe;
        if (a.isDiffuse() || b.isDiffuse())
            _lobes |= DiffuseLobe;
        if (a.isSpecular() && b.isSpecular())
            _lobes |= SpecularLobe;
        if (a.isTransmissive() || b.isTransmissive())
            _lobes |= TransmissiveLobe;
    }

    bool test(BsdfLobes lobe) const
    {
        return (_lobes & lobe._lobes) != 0;
    }

    bool isGlossy() const
    {
        return (_lobes & GlossyLobe) != 0;
    }

    bool isDiffuse() const
    {
        return (_lobes & DiffuseLobe) != 0;
    }

    bool isSpecular() const
    {
        return (_lobes & SpecularLobe) != 0;
    }

    bool isTransmissive() const
    {
        return (_lobes & TransmissiveLobe) != 0;
    }

    bool isAnisotropic() const
    {
        return (_lobes & AnisotropicLobe) != 0;
    }
};

}

#endif /* BSDFLOBES_HPP_ */
