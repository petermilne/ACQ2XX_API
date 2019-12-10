
#include "local.h"
#include <vector>
#include <string>

#include "AcqType.h"


AcqCal::AcqCal(const AcqType& _acq_type, string _base_name) :
	acq_type(_acq_type)
{

}

class DefaultAcqCal : public AcqCal {

public:
	DefaultAcqCal(const AcqType& _acq_type) :
		AcqCal(_acq_type, "")
	{}

	int getCal(int ch, double& gain_v, double& offset_v) const {
		gain_v = acq_type.getWordSize() == 2? 
				10.0/32768: 10.0/0x007fffff;
		offset_v = 0;
		return 0;
	}
};

/* cal file *.vin is the output of get.vin, looks like this:
root@acq132_082 ~ #cat /tmp/$(hostname).vin
-2.4506,2.4927,-2.4886,2.4756,-2.4958,2.4618,-2.4999,2.5065,-2.4809,2.4717,-2.4493,2.4667,-2.4927,2.4498,-2.4979,2.4805,-2.4685,2.4534,-2.5074,2.5002,-2.4390,2.4227,-2.5121,2.4736,-2.5017,2.4693,-2.4748,2.4463,-2.4587,2.4478,-2.5235,2.4732,-2.4912,2.4728,-2.4592,2.5173,-2.4918,2.4573,-2.5246,2.4947,-2.5153,2.4638,-2.4643,2.5066,-2.4645,2.5175,-2.4800,2.5012,-2.5313,2.4674,-2.5054,2.4866,-2.5396,2.4956,-2.4685,2.5002,-2.5009,2.4806,-2.4955,2.5000,-2.4573,2.5194,-2.4907,2.5321
*/
class AcqCalImpl : public AcqCal {
	Pair *pairs;
	const unsigned raw_max;
	
public:
	AcqCalImpl(const AcqType& _acq_type, string _base_name) :
		AcqCal(_acq_type, _base_name),
		raw_max(acq_type.getWordSize() == 2? 65535: 0x00ffffffU)
	{
		pairs = new Pair[acq_type.getNumChannels()];

		memset(pairs, 0, sizeof(pairs));

		FILE *fp = fopen(_base_name.c_str(), "r");
		
		assert(fp);

		for (int ic = 0; ic < acq_type.getNumChannels(); ++ic){
			if (fscanf(fp, "%f,%f,",
				&pairs[ic].p1, &pairs[ic].p2) == 2){
				dbg(2, "[%d] %f,%f", ic,
						pairs[ic].p1, pairs[ic].p2);
				continue;
			}else{
				err("early scan fail at ic=%d/%d",
						ic, acq_type.getNumChannels());
				break;
			}
		}		

		fclose(fp);	
	}

	virtual ~AcqCalImpl() {
		delete [] pairs;
	}

	int getCal(int ch, double& gain_v, double& offset_v) const {
		ch -= 1;
		gain_v = (pairs[ch].p2 - pairs[ch].p1)/raw_max;

		// (v - p1)/(r - r1) = (p2 - p1)/(r2 - r1)
		// solve for r = 0
		// v = p1 -32768 * (p2 - p1)/65535

		offset_v = pairs[ch].p1 +
				32768 * (pairs[ch].p2 - pairs[ch].p1)/65535;
		return 0;
	}
};


AcqCal* AcqCal::create(const AcqType& _acq_type, string _base_name)
{
	if (_base_name == DEFAULT_CAL){
		return new DefaultAcqCal(_acq_type);
	}else{
		return new AcqCalImpl(_acq_type, _base_name);
	}
}

void AcqCal::destroy(AcqCal* acq_cal)
{
	delete acq_cal;
}
