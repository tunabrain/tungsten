#ifndef BSDFLOBES_HPP_
#define BSDFLOBES_HPP_

#include "IntTypes.hpp"

namespace Tungsten {

class BsdfLobes
{
    uint32 _lobes;

public:
    enum Lobe
    {
        NullLobe                 = 0,
        GlossyReflectionLobe     = (1 << 0),
        GlossyTransmissionLobe   = (1 << 1),
        DiffuseReflectionLobe    = (1 << 2),
        DiffuseTransmissionLobe  = (1 << 3),
        SpecularReflectionLobe   = (1 << 4),
        SpecularTransmissionLobe = (1 << 5),
        AnisotropicLobe          = (1 << 6),
        ForwardLobe              = (1 << 7),

        GlossyLobe   =   GlossyReflectionLobe |   GlossyTransmissionLobe,
        DiffuseLobe  =  DiffuseReflectionLobe |  DiffuseTransmissionLobe,
        SpecularLobe = SpecularReflectionLobe | SpecularTransmissionLobe,

        TransmissiveLobe = GlossyTransmissionLobe | DiffuseTransmissionLobe | SpecularTransmissionLobe,
        ReflectiveLobe   = GlossyReflectionLobe   | DiffuseReflectionLobe   | SpecularReflectionLobe,

        AllLobes = TransmissiveLobe | ReflectiveLobe | AnisotropicLobe,
        AllButSpecular = ~(SpecularLobe | ForwardLobe),
    };

    BsdfLobes() = default;

    BsdfLobes(const BsdfLobes &a, const BsdfLobes &b)
    : _lobes(a._lobes | b._lobes)
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

    bool test(BsdfLobes lobe) const
    {
        return (_lobes & lobe._lobes) != 0;
    }

    bool hasGlossy() const
    {
        return (_lobes & GlossyLobe) != 0;
    }

    bool hasDiffuse() const
    {
        return (_lobes & DiffuseLobe) != 0;
    }

    bool hasSpecular() const
    {
        return (_lobes & SpecularLobe) != 0;
    }

    bool hasForward() const
    {
        return (_lobes & ForwardLobe) != 0;
    }

    bool isPureGlossy() const
    {
        return _lobes != 0 && (_lobes & ~GlossyLobe) == 0;
    }

    bool isPureSpecular() const
    {
        return _lobes != 0 && (_lobes & ~SpecularLobe) == 0;
    }

    bool isPureDiffuse() const
    {
        return _lobes != 0 && (_lobes & ~DiffuseLobe) == 0;
    }

    bool isTransmissive() const
    {
        return (_lobes & TransmissiveLobe) != 0;
    }

    bool isAnisotropic() const
    {
        return (_lobes & AnisotropicLobe) != 0;
    }

    bool isForward() const
    {
        return _lobes == ForwardLobe;
    }

    bool isPureDirac() const
    {
        return _lobes != 0 && (_lobes & AllButSpecular) == 0;
    }
};

}

#endif /* BSDFLOBES_HPP_ */
