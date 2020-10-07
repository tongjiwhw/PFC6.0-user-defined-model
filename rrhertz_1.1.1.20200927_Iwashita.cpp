// model.cpp
#include "model.h"

#include "module/interface/icontactmechanical.h"
#include "module/interface/icontact.h"
#include "module/interface/ipiecemechanical.h"
#include "module/interface/ipiece.h"
#include "module/interface/ifishcalllist.h"

#include "version.txt"

#include "utility/src/tptr.h"
#include "shared/src/mathutil.h"

#include "kernel/interface/iprogram.h"
#include "module/interface/icontactthermal.h"
#include "contactmodel/src/contactmodelthermal.h"

#ifdef rrHertz_LIB
int __stdcall DllMain(void *, unsigned, void *) {
	return 1;
}

extern "C" EXPORT_TAG const char *getName() {
#if DIM==3
	return "contactmodelmechanical3drrHertz";
#else
	return "contactmodelmechanical2drrHertz";
#endif
}

extern "C" EXPORT_TAG unsigned getMajorVersion() {
	return MAJOR_VERSION;
}

extern "C" EXPORT_TAG unsigned getMinorVersion() {
	return MINOR_VERSION;
}

extern "C" EXPORT_TAG void *createInstance() {
	cmodelsxd::ContactModelrrHertz *m = NEWC(cmodelsxd::ContactModelrrHertz());
	return (void *)m;
}
#endif 

namespace cmodelsxd {

	static const quint32 shearMask = 0x00000002;
	static const quint32 poissMask = 0x00000004;
	static const quint32 fricMask = 0x00000008;
	static const quint32 resFricMask = 0x00004000;

	using namespace itasca;

	int ContactModelrrHertz::index_ = -1;
	UInt ContactModelrrHertz::getMinorVersion() const { return MINOR_VERSION; }

	ContactModelrrHertz::ContactModelrrHertz() : inheritanceField_(shearMask | poissMask | fricMask | resFricMask)
		, hz_shear_(0.0)
		, hz_poiss_(0.0)
		, fric_(0.0)
		, hz_alpha_(1.5)
		, hz_slip_(false)
		, hz_mode_(0)
		, hz_F_(DVect(0.0))
		, rgap_(0.0)
		, dpProps_(0)
		, energies_(0)
		, hn_(0.0)
		, hs_(0.0)
		, effectiveTranslationalStiffness_(DVect2(0.0))
		, effectiveRotationalStiffness_(DAVect(0.0))
		, res_fric_(0.0)
		, res_M_(DAVect(0.0))
		, res_S_(false)
		, kr_(0.0)
		, fr_(0.0)
	{
	}

	ContactModelrrHertz::~ContactModelrrHertz() {
		// Îö¹¹º¯Êý£ºMake sure to clean up after yourself!
		if (dpProps_)
			delete dpProps_;
		if (energies_)
			delete energies_;
	}

	void ContactModelrrHertz::archive(ArchiveStream &stream) {
		// The stream allows one to archive the values of the contact model
		// so that it can be saved and restored. The minor version can be
		// used here to allow for incremental changes to the contact model too. 
		stream & hz_shear_;
		stream & hz_poiss_;
		stream & fric_;
		stream & hz_alpha_;
		stream & hz_slip_;
		stream & hz_mode_;
		stream & hz_F_;
		stream & hn_;
		stream & hs_;
		stream & rgap_;
		stream & res_fric_;
		stream & res_M_;
		stream & res_S_;
		stream & kr_;
		stream & fr_;


		if (stream.getArchiveState() == ArchiveStream::Save) {
			bool b = false;
			if (dpProps_) {
				b = true;
				stream & b;
				stream & dpProps_->dp_nratio_;
				stream & dpProps_->dp_sratio_;
				stream & dpProps_->dp_mode_;
				stream & dpProps_->dp_F_;
				stream & dpProps_->dp_alpha_;
			}
			else
				stream & b;

			b = false;
			if (energies_) {
				b = true;
				stream & b;
				stream & energies_->estrain_;
				stream & energies_->errstrain_;
				stream & energies_->eslip_;
				stream & energies_->errslip_;
				stream & energies_->edashpot_;
			}
			else
				stream & b;
		}
		else {
			bool b(false);
			stream & b;
			if (b) {
				if (!dpProps_)
					dpProps_ = NEWC(dpProps());
				stream & dpProps_->dp_nratio_;
				stream & dpProps_->dp_sratio_;
				stream & dpProps_->dp_mode_;
				stream & dpProps_->dp_F_;
				if (stream.getRestoreVersion() >= 2)
					stream & dpProps_->dp_alpha_;
			}
			stream & b;
			if (b) {
				if (!energies_)
					energies_ = NEWC(Energies());
				stream & energies_->estrain_;
				stream & energies_->errstrain_;
				stream & energies_->eslip_;
				stream & energies_->errslip_;
				stream & energies_->edashpot_;
			}
		}

		stream & inheritanceField_;
		stream & effectiveTranslationalStiffness_;
		stream & effectiveRotationalStiffness_;

		//if (stream.getArchiveState() == ArchiveStream::Save || stream.getRestoreVersion() >= 2)
		//	stream & rgap_;

	}

	void ContactModelrrHertz::copy(const ContactModel *cm) {
		// Copy all of the contact model properties. Used in the CMAT 
		// when a new contact is created. 
		ContactModelMechanical::copy(cm);
		const ContactModelrrHertz *in = dynamic_cast<const ContactModelrrHertz*>(cm);
		if (!in) throw std::runtime_error("Internal error: contact model dynamic cast failed.");

		hz_shear(in->hz_shear());
		hz_poiss(in->hz_poiss());
		fric(in->fric());
		hz_alpha(in->hz_alpha());
		hz_S(in->hz_S());
		hz_mode(in->hz_mode());
		hz_F(in->hz_F());
		hn(in->hn());
		hs(in->hs());
		rgap(in->rgap());
		res_fric(in->res_fric());
		res_M(in->res_M());
		res_S(in->res_S());
		kr(in->kr());
		fr(in->fr());

		if (in->hasDamping()) {
			if (!dpProps_)
				dpProps_ = NEWC(dpProps());
			dp_nratio(in->dp_nratio());
			dp_sratio(in->dp_sratio());
			dp_mode(in->dp_mode());
			dp_F(in->dp_F());
			dp_alpha(in->dp_alpha());
		}
		if (in->hasEnergies()) {
			if (!energies_)
				energies_ = NEWC(Energies());
			estrain(in->estrain());
			errstrain(in->errstrain());
			eslip(in->eslip());
			errslip(in->errslip());
			edashpot(in->edashpot());
		}
		inheritanceField(in->inheritanceField());
		effectiveTranslationalStiffness(in->effectiveTranslationalStiffness());
		effectiveRotationalStiffness(in->effectiveRotationalStiffness());
	}

	QVariant ContactModelrrHertz::getProperty(uint i, const IContact *) const {
		// Return the property. The IContact pointer is provided so that 
		// more complicated properties, depending on contact characteristics,
		// can be calcualted.
		QVariant var;
		switch (i) {
		case kwHzShear:   return hz_shear_;
		case kwHzPoiss:   return hz_poiss_;
		case kwFric:      return fric_;
		case kwHzAlpha:   return hz_alpha_;
		case kwHzS:       return hz_slip_;
		case kwHzSd:      return hz_mode_;
		case kwHzF:       var.setValue(hz_F_); return var;
		case kwRGap:      return rgap_;
		case kwDpNRatio:  return dpProps_ ? dpProps_->dp_nratio_ : 0.0;
		case kwDpSRatio:  return dpProps_ ? dpProps_->dp_sratio_ : 0.0;
		case kwDpMode:    return dpProps_ ? dpProps_->dp_mode_ : 0;
		case kwDpAlpha:   return dpProps_ ? dpProps_->dp_alpha_ : 0.0;
		case kwDpF: {
			dpProps_ ? var.setValue(dpProps_->dp_F_) : var.setValue(DVect(0.0));
			return var;
		}
		case kwResFric:     return res_fric_;
		case kwResMoment:   var.setValue(res_M_); return var;
		case kwResS:        return res_S_;
		case kwResKr:       return kr_;
		}
		assert(0);
		return QVariant();
	}

	bool ContactModelrrHertz::getPropertyGlobal(uint i) const {
		// Returns whether or not a property is held in the global axis system (TRUE)
		// or the local system (FALSE). Used by the plotting logic.
		switch (i) {
		case kwHzF: // fall through   
		case kwDpF:
		case kwResMoment:
			return false;
		}
		return true;
	}

	bool ContactModelrrHertz::setProperty(uint i, const QVariant &v, IContact *) {
		// Set a contact model property. Return value indicates that the timestep
		// should be recalculated. 
		dpProps dp;
		switch (i) {
		case kwHzShear: {
			if (!v.canConvert<double>())
				throw Exception("hz_shear must be a double.");
			double val(v.toDouble());
			if (val < 0.0)
				throw Exception("Negative shear modulus (hz_shear) not allowed.");
			hz_shear_ = val;
			return true;
		}
		case kwHzPoiss: {
			if (!v.canConvert<double>())
				throw Exception("hz_poiss must be a double.");
			double val(v.toDouble());
			if (val <= -1.0 || val > 0.5)
				throw Exception("Poisson ratio (hz_poiss) must be in range (-1.0,0.5].");
			hz_poiss_ = val;
			return true;
		}
		case kwFric: {
			if (!v.canConvert<double>())
				throw Exception("fric must be a double.");
			double val(v.toDouble());
			if (val < 0.0)
				throw Exception("Negative fric not allowed.");
			fric_ = val;
			return false;
		}
		case kwHzAlpha: {
			if (!v.canConvert<double>())
				throw Exception("hz_alpha must be a double.");
			double val(v.toDouble());
			if (val <= 0.0)
				throw Exception("Negative exponent value not allowed.");
			hz_alpha_ = val;
			return false;
		}
		case kwHzSd: {
			if (!v.canConvert<uint>())
				throw Exception("hz_mode must be 0 or 1.");
			uint val(v.toUInt());
			if (val > 1)
				throw Exception("hz_mode must be 0 or 1.");
			hz_mode_ = val;
			return false;
		}
		case kwDpF: {
			if (!v.canConvert<DVect>())
				throw Exception("dp_force must be a vector.");
			DVect val(v.value<DVect>());
			if (val.fsum() == 0.0 && !dpProps_)
				return false;
			if (!dpProps_)
				dpProps_ = NEWC(dpProps());
			dpProps_->dp_F_ = val;
			return false;
		}
		case kwRGap: {
			if (!v.canConvert<double>())
				throw Exception("Reference gap must be a double.");
			double val(v.toDouble());
			rgap_ = val;
			return false;
		}
		case kwDpNRatio: {
			if (!v.canConvert<double>())
				throw Exception("dp_nratio must be a double.");
			double val(v.toDouble());
			if (val < 0.0)
				throw Exception("Negative dp_nratio not allowed.");
			if (val == 0.0 && !dpProps_)
				return false;
			if (!dpProps_)
				dpProps_ = NEWC(dpProps());
			dpProps_->dp_nratio_ = val;
			return true;
		}
		case kwDpSRatio: {
			if (!v.canConvert<double>())
				throw Exception("dp_sratio must be a double.");
			double val(v.toDouble());
			if (val < 0.0)
				throw Exception("Negative dp_sratio not allowed.");
			if (val == 0.0 && !dpProps_)
				return false;
			if (!dpProps_)
				dpProps_ = NEWC(dpProps());
			dpProps_->dp_sratio_ = val;
			return true;
		}
		case kwDpMode: {
			if (!v.canConvert<int>())
				throw Exception("The viscous mode dp_mode must be 0, 1, 2, or 3.");
			int val(v.toInt());
			if (val == 0 && !dpProps_)
				return false;
			if (val < 0 || val > 3)
				throw Exception("The viscous mode dp_mode must be 0, 1, 2, or 3.");
			if (!dpProps_)
				dpProps_ = NEWC(dpProps());
			dpProps_->dp_mode_ = val;
			return false;
		}
		case kwDpAlpha: {
			if (!v.canConvert<double>())
				throw Exception("dp_alpha must be a double.");
			double val(v.toDouble());
			if (val < 0.0)
				throw Exception("Negative dp_alpha not allowed.");
			if (val == 0.0 && !dpProps_)
				return false;
			if (!dpProps_)
				dpProps_ = NEWC(dpProps());
			dpProps_->dp_alpha_ = val;
			return true;
		}
		case kwHzF: {
			if (!v.canConvert<DVect>())
				throw Exception("hz_force must be a vector.");
			DVect val(v.value<DVect>());
			hz_F_ = val;
			return false;
		}

		case kwResFric: {
			if (!v.canConvert<double>())
				throw Exception("res_fric must be a double.");
			double val(v.toDouble());
			if (val < 0.0)
				throw Exception("Negative res_fric not allowed.");
			res_fric_ = val;
			return false;
		}

		case kwResMoment: {
			DAVect val(0.0);
#ifdef TWOD               
			if (!v.canConvert<DAVect>() && !v.canConvert<double>())
				throw Exception("res_moment must be an angular vector.");
			if (v.canConvert<DAVect>())
				val = DAVect(v.value<DAVect>());
			else
				val = DAVect(v.toDouble());
#else
			if (!v.canConvert<DAVect>() && !v.canConvert<DVect>())
				throw Exception("res_moment must be an angular vector.");
			if (v.canConvert<DAVect>())
				val = DAVect(v.value<DAVect>());
			else
				val = DAVect(v.value<DVect>());
#endif
			res_M_ = val;
			return false;
		}
		}
		return false;
	}

	bool ContactModelrrHertz::getPropertyReadOnly(uint i) const {
		// Returns TRUE if a property is read only or FALSE otherwise. 
		switch (i) {
			//      case kwHzF:
		case kwDpF:
		case kwHzS:
		case kwResS:
		case kwResKr:
			return true;
		default:
			break;
		}
		return false;
	}

	bool ContactModelrrHertz::supportsInheritance(uint i) const {
		// Returns TRUE if a property supports inheritance or FALSE otherwise. 
		switch (i) {
		case kwHzShear:
		case kwHzPoiss:
		case kwFric:
		case kwResFric:
			return true;
		default:
			break;
		}
		return false;
	}

	double ContactModelrrHertz::getEnergy(uint i) const {
		// Return an energy value. 
		double ret(0.0);
		if (!energies_)
			return ret;
		switch (i) {
		case kwEStrain:    return energies_->estrain_;
		case kwERRStrain:  return energies_->errstrain_;
		case kwESlip:      return energies_->eslip_;
		case kwERRSlip:    return energies_->errslip_;
		case kwEDashpot:   return energies_->edashpot_;
		}
		assert(0);
		return ret;
	}

	bool ContactModelrrHertz::getEnergyAccumulate(uint i) const {
		// Returns TRUE if the corresponding energy is accumulated or FALSE otherwise.
		switch (i) {
		case kwEStrain:   return false;
		case kwERRStrain: return false;
		case kwESlip:     return true;
		case kwERRSlip:   return true;
		case kwEDashpot:  return true;
		}
		assert(0);
		return false;
	}

	void ContactModelrrHertz::setEnergy(uint i, const double &d) {
		// Set an energy value. 
		if (!energies_) return;
		switch (i) {
		case kwEStrain:    energies_->estrain_ = d;   return;
		case kwERRStrain:  energies_->errstrain_ = d; return;
		case kwESlip:      energies_->eslip_ = d;   return;
		case kwERRSlip:    energies_->errslip_ = d; return;
		case kwEDashpot:   energies_->edashpot_ = d;   return;
		}
		assert(0);
		return;
	}

	bool ContactModelrrHertz::validate(ContactModelMechanicalState *state, const double &) {
		// Validate the / Prepare for entry into ForceDispLaw. The validate function is called when:
		// (1) the contact is created, (2) a property of the contact that returns a true via
		// the setProperty method has been modified and (3) when a set of cycles is executed
		// via the {cycle N} command.
		// Return value indicates contact activity (TRUE: active, FALSE: inactive).
		assert(state);
		const IContactMechanical *c = state->getMechanicalContact();
		assert(c);

		if (state->trackEnergy_)
			activateEnergy();

		updateStiffCoef(c);
		if ((inheritanceField_ & shearMask) || (inheritanceField_ & poissMask))
			updateEndStiffCoef(c);

		if (inheritanceField_ & fricMask)
			updateEndFric(c);

		if (inheritanceField_ & resFricMask)
			updateResFric(c);

		updateEffectiveStiffness(state);
		return checkActivity(state->gap_);
	}

	bool ContactModelrrHertz::updateStiffCoef(const IContactMechanical *con) {
		double hnold = hn_;
		double hsold = hs_;
		double c12 = con->getEnd1Curvature().y();
		double c22 = con->getEnd2Curvature().y();
		double reff = c12 + c22;
		if (reff == 0.0)
			throw Exception("rrHertz contact model undefined for 2 non-curved surfaces");
		reff = 2.0 / reff;
		hn_ = 2.0 / 3.0 * (hz_shear_ / (1 - hz_poiss_)) * sqrt(2.0*reff);
		hs_ = (2.0*(1 - hz_poiss_) / (2.0 - hz_poiss_))*hz_alpha_*pow(hn_, 1.0 / hz_alpha_);
		return ((hn_ != hnold) || (hs_ != hsold));
	}


	static const QString gstr("hz_shear");
	static const QString nustr("hz_poiss");

	bool ContactModelrrHertz::updateEndStiffCoef(const IContactMechanical *con) {
		assert(con);
		double g1 = hz_shear_;
		double g2 = hz_shear_;
		double nu1 = hz_poiss_;
		double nu2 = hz_poiss_;
		QVariant vg1 = con->getEnd1()->getProperty(gstr);
		QVariant vg2 = con->getEnd2()->getProperty(gstr);
		QVariant vnu1 = con->getEnd1()->getProperty(nustr);
		QVariant vnu2 = con->getEnd2()->getProperty(nustr);
		if (vg1.isValid() && vg2.isValid()) {
			g1 = vg1.toDouble();
			g2 = vg2.toDouble();
			if (g1 < 0.0 || g2 < 0.0)
				throw Exception("Negative shear modulus not allowed in rrHertz contact model");
		}
		if (vnu1.isValid() && vnu2.isValid()) {
			nu1 = vnu1.toDouble();
			nu2 = vnu2.toDouble();
			if (nu1 <= -1.0 || nu1 > 0.5 || nu2 <= -1.0 || nu2 > 0.5)
				throw Exception("Poisson ratio should be in range (-1.0,0.5] in rrHertz contact model");
		}
		if (g1*g2 == 0.0) return false;
		double es = 1.0 / ((1.0 - nu1) / (2.0*g1) + (1.0 - nu2) / (2.0*g2));
		double gs = 1.0 / ((2.0 - nu1) / g1 + (2.0 - nu2) / g2);
		hz_poiss_ = (4.0*gs - es) / (2.0*gs - es);
		hz_shear_ = 2.0*gs*(2 - hz_poiss_);
		if (hz_shear_ < 0.0)
			throw Exception("Negative shear modulus not allowed in Hertz contact model");
		if (hz_poiss_ <= -1.0 || hz_poiss_ > 0.5)
			throw Exception("Poisson ratio should be in range (-1.0,0.5] in Hertz contact model");
		return updateStiffCoef(con);
	}

	static const QString fricstr("fric");
	bool ContactModelrrHertz::updateEndFric(const IContactMechanical *con) {
		assert(con);
		QVariant v1 = con->getEnd1()->getProperty(fricstr);
		QVariant v2 = con->getEnd2()->getProperty(fricstr);
		if (!v1.isValid() || !v2.isValid())
			return false;
		double fric1 = std::max(0.0, v1.toDouble());
		double fric2 = std::max(0.0, v2.toDouble());
		double val = fric_;
		fric_ = std::min(fric1, fric2);
		return ((fric_ != val));
	}

	static const QString rfricstr("rr_fric");
	bool ContactModelrrHertz::updateResFric(const IContactMechanical *con) {
		assert(con);
		QVariant v1 = con->getEnd1()->getProperty(rfricstr);
		QVariant v2 = con->getEnd2()->getProperty(rfricstr);
		if (!v1.isValid() || !v2.isValid())
			return false;
		double rfric1 = std::max(0.0, v1.toDouble());
		double rfric2 = std::max(0.0, v2.toDouble());
		double val = res_fric_;
		res_fric_ = std::min(rfric1, rfric2);
		return ((res_fric_ != val));
	}

	bool ContactModelrrHertz::endPropertyUpdated(const QString &name, const IContactMechanical *c) {
		// The endPropertyUpdated method is called whenever a surface property (with a name
		// that matches an inheritable contact model property name) of one of the contacting
		// pieces is modified. This allows the contact model to update its associated
		// properties. The return value denotes whether or not the update has affected
		// the time step computation (by having modified the translational or rotational
		// tangent stiffnesses). If true is returned, then the time step will be recomputed.  
		assert(c);
		QStringList availableProperties = getProperties().simplified().replace(" ", "").split(",", QString::SkipEmptyParts);
		QRegExp rx(name, Qt::CaseInsensitive);
		int idx = availableProperties.indexOf(rx) + 1;
		bool ret = false;

		if (idx <= 0)
			return ret;

		switch (idx) {
		case kwHzShear: {
			if (inheritanceField_ & shearMask)
				ret = updateEndStiffCoef(c);
			break;
		}
		case kwHzPoiss: {
			if (inheritanceField_ & poissMask)
				ret = updateEndStiffCoef(c);
			break;
		}
		case kwFric: {
			if (inheritanceField_ & fricMask)
				ret = updateEndFric(c);
			break;
		}
		case kwResFric: { //rr_fric
			if (inheritanceField_ & resFricMask)
				ret = updateResFric(c);
			break;
		}
		}
		return ret;
	}

	void ContactModelrrHertz::updateEffectiveStiffness(ContactModelMechanicalState *state) {
		kr_ = 0.0;
		effectiveTranslationalStiffness_ = DVect2(hn_, hs_);
		double overlap = rgap_ - state->gap_;
		if (overlap <= 0.0) return;
		// normal force in rrHertz part
		double fn = hn_ * pow(overlap, hz_alpha_);
		// tangent normal stiffness
		double kn = hz_alpha_ * hn_ * pow(overlap, hz_alpha_ - 1.0);
		// initial tangent shear stiffness 
		double ks = hs_ * pow(fn, (hz_alpha_ - 1.0) / hz_alpha_);
		// 1)first compute rolling resistance stiffness
		if (res_fric_ > 0.0) {
			double rbar = 0.0;
			double r1 = 1.0 / state->end1Curvature_.y();
			rbar = r1;
			double r2 = 0.0;
			if (state->end2Curvature_.y()) {
				r2 = 1.0 / state->end2Curvature_.y();
				rbar = (r1*r2) / (r1 + r2);
			}

			kr_ = ks * rbar*rbar;
			fr_ = res_fric_ * rbar;
		}
		// Now calculate effective stiffness
		DVect2 retT(kn, ks);
		// correction if viscous damping active
		if (dpProps_) {
			DVect2 correct(1.0);
			if (dpProps_->dp_nratio_)
				correct.rx() = sqrt(1.0 + dpProps_->dp_nratio_*dpProps_->dp_nratio_) - dpProps_->dp_nratio_;
			if (dpProps_->dp_sratio_)
				correct.ry() = sqrt(1.0 + dpProps_->dp_sratio_*dpProps_->dp_sratio_) - dpProps_->dp_sratio_;
			retT /= (correct*correct);
		}

		effectiveTranslationalStiffness_ = retT;
		effectiveRotationalStiffness_ = DAVect(kr_);
		// Effective rotational stiffness (bending only)
#if DIM==3 
		effectiveRotationalStiffness_.rx() = 0.0;
#endif

	}

	bool ContactModelrrHertz::forceDisplacementLaw(ContactModelMechanicalState *state, const double &timestep) {
		assert(state);

		if (state->activated()) {
			if (cmEvents_[fActivated] >= 0) {
				FArray<QVariant, 2> arg;
				QVariant v;
				IContact * c = const_cast<IContact*>(state->getContact());
				TPtr<IThing> t(c->getIThing());
				v.setValue(t);
				arg.push_back(v);
				IFishCallList *fi = const_cast<IFishCallList*>(state->getProgram()->findInterface<IFishCallList>());
				fi->setCMFishCallArguments(c, arg, cmEvents_[fActivated]);
			}
		}

		// Current overlap
		double overlap = rgap_ - state->gap_;
		// Relative translational increment
		DVect trans = state->relativeTranslationalIncrement_;

#ifdef THREED
		DVect norm(trans.x(), 0.0, 0.0);
#else
		DVect norm(trans.x(), 0.0);
#endif
		DAVect ang = state->relativeAngularIncrement_;
		// normal force in rrHertz part
		double fn = hn_ * pow(overlap, hz_alpha_);
		// tangent normal stiffness
		double kn = hz_alpha_ * hn_ * pow(overlap, hz_alpha_ - 1.0);
		// initial tangent shear stiffness 
		double ks = hs_ * pow(fn, (hz_alpha_ - 1.0) / hz_alpha_);


		DVect fs_old = hz_F_;
		fs_old.rx() = 0.0;

		if (hz_mode_ && fn < hz_F_.x()) {
			double ks_old = hs_ * pow(hz_F_.x(), (hz_alpha_ - 1.0) / hz_alpha_);
			double rat = ks / ks_old;
			fs_old *= rat;
		}

		DVect u_s = trans;
		u_s.rx() = 0.0;
		DVect vec = u_s * ks;

		DVect fs = fs_old - vec;

		if (state->canFail_) {
			// resolve sliding
			double crit = fn * fric_;
			double sfmag = fs.mag();
			if (sfmag > crit) {
				double rat = crit / sfmag;
				fs *= rat;
				if (!hz_slip_ && cmEvents_[fSlipChange] >= 0) {
					FArray<QVariant, 3> arg;
					QVariant p1;
					IContact * c = const_cast<IContact*>(state->getContact());
					TPtr<IThing> t(c->getIThing());
					p1.setValue(t);
					arg.push_back(p1);
					p1.setValue(0);
					arg.push_back(p1);
					IFishCallList *fi = const_cast<IFishCallList*>(state->getProgram()->findInterface<IFishCallList>());
					fi->setCMFishCallArguments(c, arg, cmEvents_[fSlipChange]);
				}
				hz_slip_ = true;
			}
			else {
				if (hz_slip_) {
					if (cmEvents_[fSlipChange] >= 0) {
						FArray<QVariant, 3> arg;
						QVariant p1;
						IContact * c = const_cast<IContact*>(state->getContact());
						TPtr<IThing> t(c->getIThing());
						p1.setValue(t);
						arg.push_back(p1);
						p1.setValue(1);
						arg.push_back(p1);
						IFishCallList *fi = const_cast<IFishCallList*>(state->getProgram()->findInterface<IFishCallList>());
						fi->setCMFishCallArguments(c, arg, cmEvents_[fSlipChange]);
					}
					hz_slip_ = false;
				}
			}
		}

		hz_F_ = fs;          // total force in hertz part
		hz_F_.rx() += fn;
		effectiveTranslationalStiffness_ = DVect2(kn, ks);
		// 3)Rolling resistance
		DAVect res_M_old = res_M_;
		if ((fr_ == 0.0) || (kr_ == 0.0)) {
			res_M_.fill(0.0);
		}
		else {
			DAVect angStiff(0.0);
			DAVect MomentInc(0.0);
#if DIM==3 
			angStiff.rx() = 0.0;
			angStiff.ry() = kr_;
#endif
			angStiff.rz() = kr_;
			MomentInc = ang * angStiff * (-1.0);
			res_M_ += MomentInc;
			if (state->canFail_) {
				// Account for bending strength
				double rmax = std::abs(fr_*hz_F_.x());
				double rmag = res_M_.mag();
				if (rmag > rmax) {
					double fac = rmax / rmag;
					res_M_ *= fac;
					res_S_ = true;
				}
				else {
					res_S_ = false;
				}
			}
		}
		// 4) Account for dashpot forces
		if (dpProps_) {
			dpProps_->dp_F_.fill(0.0);
			double vcn(0.0), vcs(0.0);
			setDampCoefficients(*state, &vcn, &vcs);
			double fac = 1.0;
			if (dpProps_->dp_alpha_ > 0.0) fac = pow(overlap, dpProps_->dp_alpha_);
			// First damp all components
			dpProps_->dp_F_ = u_s * (-1.0* vcs*fac) / timestep; // shear component   
			dpProps_->dp_F_ -= norm * vcn*fac / timestep;       // normal component
			// Need to change behavior based on the dp_mode
			if ((dpProps_->dp_mode_ == 1 || dpProps_->dp_mode_ == 3)) {
				// limit the tensile if not bonded
				if (dpProps_->dp_F_.x() + hz_F_.x() < 0)
					dpProps_->dp_F_.rx() = -hz_F_.rx();
			}
			if (hz_slip_ && dpProps_->dp_mode_ > 1) {
				// limit the shear if not sliding
				double dfn = dpProps_->dp_F_.rx();
				dpProps_->dp_F_.fill(0.0);
				dpProps_->dp_F_.rx() = dfn;
			}
			// Correct effective translational stiffness
			DVect2 correct(1.0);
			if (dpProps_->dp_nratio_)
				correct.rx() = sqrt(1.0 + dpProps_->dp_nratio_*dpProps_->dp_nratio_) - dpProps_->dp_nratio_;
			if (dpProps_->dp_sratio_)
				correct.ry() = sqrt(1.0 + dpProps_->dp_sratio_*dpProps_->dp_sratio_) - dpProps_->dp_sratio_;
			effectiveTranslationalStiffness_ /= (correct*correct);
		}

		// 5) Compute energies
		if (state->trackEnergy_) {
			assert(energies_);
			energies_->estrain_ = 0.0;
			if (kn)
				energies_->estrain_ = hz_alpha_ * hz_F_.x()*hz_F_.x() / ((hz_alpha_ + 1.0)*kn);
			if (ks) {
				double smag2 = fs.mag2();
				energies_->estrain_ += 0.5*smag2 / ks;

				if (hz_slip_) {
					DVect avg_F_s = (fs + fs_old)*0.5;
					DVect u_s_el = (fs - fs_old) / ks;
					energies_->eslip_ -= std::min(0.0, (avg_F_s | (u_s + u_s_el)));
				}
			}

			// Add the rolling resistance energy contributions.
			energies_->errstrain_ = 0.0;
			if (kr_) {
				energies_->errstrain_ = 0.5*res_M_.mag2() / kr_;
				if (res_S_) {
					// If sliding calculate the slip energy and accumulate it.
					DAVect avg_M = (res_M_ + res_M_old)*0.5;
					DAVect t_s_el = (res_M_ - res_M_old) / kr_;
					energies_->errslip_ -= std::min(0.0, (avg_M | (ang + t_s_el)));
				}

			}
			if (dpProps_) {
				// Calculate damping energy (accumulated) if the dashpots are active. 
				energies_->edashpot_ -= dpProps_->dp_F_ | trans;
			}
		}
		// assert(hz_F_ == hz_F_);
		return true;

	}

	void ContactModelrrHertz::setForce(const DVect &v, IContact *c) {
		hz_F(v);
		if (v.x() > 0)
			rgap_ = c->getGap() + pow(v.x() / hn_, 1. / hz_alpha_);
	}

	void ContactModelrrHertz::propagateStateInformation(IContactModelMechanical* old, const CAxes &oldSystem, const CAxes &newSystem) {
		// Only called for contacts with wall facets when the wall resolution scheme
		// is set to full!
		// Only do something if the contact model is of the same type
		if (old->getContactModel()->getName().compare("rrhertz", Qt::CaseInsensitive) == 0) {
			ContactModelrrHertz *oldCm = (ContactModelrrHertz *)old;
#ifdef THREED
			// Need to rotate just the shear component from oldSystem to newSystem

			// Step 1 - rotate oldSystem so that the normal is the same as the normal of newSystem
			DVect axis = oldSystem.e1() & newSystem.e1();
			double c, ang, s;
			DVect re2;
			if (!checktol(axis.abs().maxComp(), 0.0, 1.0, 1000)) {
				axis = axis.unit();
				c = oldSystem.e1() | newSystem.e1();
				if (c > 0)
					c = std::min(c, 1.0);
				else
					c = std::max(c, -1.0);
				ang = acos(c);
				s = sin(ang);
				double t = 1. - c;
				DMatrix<3, 3> rm;
				rm.get(0, 0) = t * axis.x()*axis.x() + c;
				rm.get(0, 1) = t * axis.x()*axis.y() - axis.z()*s;
				rm.get(0, 2) = t * axis.x()*axis.z() + axis.y()*s;
				rm.get(1, 0) = t * axis.x()*axis.y() + axis.z()*s;
				rm.get(1, 1) = t * axis.y()*axis.y() + c;
				rm.get(1, 2) = t * axis.y()*axis.z() - axis.x()*s;
				rm.get(2, 0) = t * axis.x()*axis.z() - axis.y()*s;
				rm.get(2, 1) = t * axis.y()*axis.z() + axis.x()*s;
				rm.get(2, 2) = t * axis.z()*axis.z() + c;
				re2 = rm * oldSystem.e2();
			}
			else
				re2 = oldSystem.e2();

			// Step 2 - get the angle between the oldSystem rotated shear and newSystem shear
			axis = re2 & newSystem.e2();
			DVect2 tpf;
			DVect2 tpm;
			DMatrix<2, 2> m;
			if (!checktol(axis.abs().maxComp(), 0.0, 1.0, 1000)) {
				axis = axis.unit();
				c = re2 | newSystem.e2();
				if (c > 0)
					c = std::min(c, 1.0);
				else
					c = std::max(c, -1.0);
				ang = acos(c);
				if (!checktol(axis.x(), newSystem.e1().x(), 1.0, 100))
					ang *= -1;
				s = sin(ang);
				m.get(0, 0) = c;
				m.get(1, 0) = s;
				m.get(0, 1) = -m.get(1, 0);
				m.get(1, 1) = m.get(0, 0);
				tpf = m * DVect2(oldCm->hz_F_.y(), oldCm->hz_F_.z());
				tpm = m * DVect2(oldCm->res_M_.y(), oldCm->res_M_.z());
			}
			else {
				m.get(0, 0) = 1.;
				m.get(0, 1) = 0.;
				m.get(1, 0) = 0.;
				m.get(1, 1) = 1.;
				tpf = DVect2(oldCm->hz_F_.y(), oldCm->hz_F_.z());
				tpm = DVect2(oldCm->res_M_.y(), oldCm->res_M_.z());
			}
			DVect pforce = DVect(0, tpf.x(), tpf.y());
			DVect pm = DVect(0, tpm.x(), tpm.y());
#else
			oldSystem;
			newSystem;
			DVect pforce = DVect(0, oldCm->hz_F_.y());
			DVect pm = DVect(0, oldCm->res_M_.y());
#endif
			for (int i = 1; i < dim; ++i)
				hz_F_.rdof(i) += pforce.dof(i);
			oldCm->hz_F_ = DVect(0.0);
			oldCm->res_M_ = DAVect(0.0);
			if (dpProps_ && oldCm->dpProps_) {
#ifdef THREED
				tpf = m * DVect2(oldCm->dpProps_->dp_F_.y(), oldCm->dpProps_->dp_F_.z());
				pforce = DVect(oldCm->dpProps_->dp_F_.x(), tpf.x(), tpf.y());
#else
				pforce = oldCm->dpProps_->dp_F_;
#endif
				dpProps_->dp_F_ += pforce;
				oldCm->dpProps_->dp_F_ = DVect(0.0);
			}

			if (oldCm->getEnergyActivated()) {
				activateEnergy();
				energies_->estrain_ = oldCm->energies_->estrain_;
				energies_->eslip_ = oldCm->energies_->eslip_;
				energies_->edashpot_ = oldCm->energies_->edashpot_;
				oldCm->energies_->estrain_ = 0.0;
				oldCm->energies_->eslip_ = 0.0;
				oldCm->energies_->edashpot_ = 0.0;
			}
			rgap_ = oldCm->rgap_;
		}
		assert(hz_F_ == hz_F_);
	}

	void ContactModelrrHertz::setNonForcePropsFrom(IContactModel *old) {
		// Only called for contacts with wall facets when the wall resolution scheme
		// is set to full!
		// Only do something if the contact model is of the same type
		if (old->getName().compare("rrhertz", Qt::CaseInsensitive) == 0 && !isBonded()) {
			ContactModelrrHertz *oldCm = (ContactModelrrHertz *)old;
			hn_ = oldCm->hn_;
			hs_ = oldCm->hs_;
			fric_ = oldCm->fric_;
			rgap_ = oldCm->rgap_;
			res_fric_ = oldCm->res_fric_;
			res_S_ = oldCm->res_S_;
			kr_ = oldCm->kr_;
			fr_ = oldCm->fr_;

			if (oldCm->dpProps_) {
				if (!dpProps_)
					dpProps_ = NEWC(dpProps());
				dpProps_->dp_nratio_ = oldCm->dpProps_->dp_nratio_;
				dpProps_->dp_sratio_ = oldCm->dpProps_->dp_sratio_;
				dpProps_->dp_mode_ = oldCm->dpProps_->dp_mode_;
			}
		}
	}

	DVect ContactModelrrHertz::getForce(const IContactMechanical *) const {
		DVect ret(hz_F_);
		if (dpProps_)
			ret += dpProps_->dp_F_;
		return ret;
	}

	DAVect ContactModelrrHertz::getMomentOn1(const IContactMechanical *c) const {
		DVect force = getForce(c);
		DAVect ret(res_M_);
		c->updateResultingTorqueOn1Local(force, &ret);
		return ret;
	}

	DAVect ContactModelrrHertz::getMomentOn2(const IContactMechanical *c) const {
		DVect force = getForce(c);
		DAVect ret(res_M_);
		c->updateResultingTorqueOn2Local(force, &ret);
		return ret;
	}

	void ContactModelrrHertz::setDampCoefficients(const ContactModelMechanicalState &state, double *vcn, double *vcs) {
		double overlap = rgap_ - state.gap_;
		double kn = hz_alpha_ * hn_*pow(overlap, hz_alpha_ - 1.0);
		double ks = hs_ * pow(hz_F_.x(), (hz_alpha_ - 1.0) / hz_alpha_);
		*vcn = dpProps_->dp_nratio_ * 2.0 * sqrt(state.inertialMass_*(kn));
		*vcs = dpProps_->dp_sratio_ * 2.0 * sqrt(state.inertialMass_*(ks));
	}

} // namespace cmodelsxd
	// EoF