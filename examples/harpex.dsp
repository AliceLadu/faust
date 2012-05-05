//-----------------------------------------------
// 		Basic harpe simulation with OSC control
//		(based on Karplus-Strong)
//
//-----------------------------------------------

declare name  	"Harpe";
declare author  "Grame";

import("math.lib");
import("filter.lib");
downfilter = lowpass(8,18000);
process = harpe(11) : par(i,2, downfilter); 	// an 11 strings harpe



//-----------------------------------------------
// 		String simulation
//-----------------------------------------------
string(freq, att, level, trig) = noise*level
							: *(trig : trigger(freq2samples(freq)))
							: resonator(freq2samples(freq), att)
	with {
		resonator(d, a) = (+ : @(d-1)) ~ (average : *(1.0-a));
		average(x)	= (x+x')/2;
		trigger(n) 	= upfront : + ~ decay(n) : >(0.0);
		upfront(x) 	= (x-x') > 0.0;
		decay(n,x)	= x - (x>0.0)/n;
		freq2samples(f) = SR/f;
	};


//-----------------------------------------------
// 		Build a N strings harpe
//		Each string is triggered by a specific
//		position [0..1] of the "hand"
//-----------------------------------------------
harpe(N) = 	hand <: par(i, N, position((i+0.5)/N)
							: string( 440 * 2.0^(i/5.0), att, lvl)
							: pan((i+0.5)/N) )
				 :> _,_
	with {
		lvl  = hslider("level [unit:f][osc:/accxyz/0 -10 10]", 0.5, 0, 1, 0.01)^2;
		att  = hslider("attenuation [osc:/1/fader3]", 0.005, 0, 0.01, 0.001);
		hand = hslider("hand[osc:/accxyz/1 -10 10]", 0, 0, 1, 0.01);
		pan(p) = _ <: *(sqrt(1-p)), *(sqrt(p));
		position(a,x) = (min(x,x') < a) & (a < max(x, x'));
	};
