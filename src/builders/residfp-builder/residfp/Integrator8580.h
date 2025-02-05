/*
 * This file is part of libsidplayfp, a SID player engine.
 *
 * Copyright 2011-2020 Leandro Nini <drfiemost@users.sourceforge.net>
 * Copyright 2007-2010 Antti Lankila
 * Copyright 2004, 2010 Dag Lem <resid@nimrod.no>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef INTEGRATOR8580_H
#define INTEGRATOR8580_H

#include <stdint.h>
#include <cassert>

#include "siddefs-fp.h"

namespace reSIDfp
{

/**
 * 8580 integrator
 *
 *                   +---C---+
 *                   |       |
 *     vi -----Rfc---o--[A>--o-- vo
 *                   vx
 *
 *     IRfc + ICr = 0
 *     IRfc + C*(vc - vc0)/dt = 0
 *     dt/C*(IRfc) + vc - vc0 = 0
 *     vc = vc0 - n*(IRfc(vi,vx))
 *     vc = vc0 - n*(IRfc(vi,g(vc)))
 *
 * IRfc = K*W/L*(Vgst^2 - Vgdt^2) = n*((Vddt - vx)^2 - (Vddt - vi)^2)
 *
 * Rfc gate voltage is generated by an OP Amp and depends on chip temperature.
 */
class Integrator8580
{
private:
    const unsigned short* opamp_rev;

    mutable int vx;
    mutable int vc;

    unsigned short nVgt;
    unsigned short n_dac;

    const double voice_DC_voltage;
    const double Vth;
    const double nKp;
    const double vmin;
    const double N16;

public:
    Integrator8580(const unsigned short* opamp_rev, double vdv, double Vth, double nKp, double vmin, double N16) :
        opamp_rev(opamp_rev),
        voice_DC_voltage(vdv),
        vx(0),
        vc(0),
        Vth(Vth),
        nKp(nKp),
        vmin(vmin),
        N16(N16)
    {
        setV(1.5);
    }

    void setFc(double wl)
    {
        // Normalized current factor, 1 cycle at 1MHz.
        // Fit in 5 bits.
        const double tmp = (1 << 13) * nKp * wl;
        assert(tmp > -0.5 && tmp < 65535.5);
        n_dac = static_cast<unsigned short>(tmp + 0.5);
    }

    /**
     * Set FC gate voltage multiplier.
     */
    void setV(double v)
    {
        // Gate voltage is controlled by the switched capacitor voltage divider
        // Ua = Ue * v = 4.76v  1<v<2
        assert(v > 1.0 && v < 2.0);
        const double Vg = voice_DC_voltage * v;
        const double Vgt = Vg - Vth;

        // Vg - Vth, normalized so that translated values can be subtracted:
        // Vgt - x = (Vgt - t) - (x - t)
        const double tmp = N16 * (Vgt - vmin);
        assert(tmp > -0.5 && tmp < 65535.5);
        nVgt = static_cast<unsigned short>(tmp + 0.5);
    }

    int solve(int vi) const;
};

} // namespace reSIDfp

#if RESID_INLINING || defined(INTEGRATOR8580_CPP)

namespace reSIDfp
{

RESID_INLINE
int Integrator8580::solve(int vi) const
{
    // Make sure we're not in subthreshold mode
    assert(vx < nVgt);

    // DAC voltages
    const unsigned int Vgst = nVgt - vx;
    const unsigned int Vgdt = (vi < nVgt) ? nVgt - vi : 0;  // triode/saturation mode

    const unsigned int Vgst_2 = Vgst * Vgst;
    const unsigned int Vgdt_2 = Vgdt * Vgdt;

    // DAC current, scaled by (1/m)*2^13*m*2^16*m*2^16*2^-15 = m*2^30
    const int n_I_dac = n_dac * (static_cast<int>(Vgst_2 - Vgdt_2) >> 15);

    // Change in capacitor charge.
    vc += n_I_dac;

    // vx = g(vc)
    const int tmp = (vc >> 15) + (1 << 15);
    assert(tmp < (1 << 16));
    vx = opamp_rev[tmp];

    // Return vo.
    return vx - (vc >> 14);
}

} // namespace reSIDfp

#endif

#endif
