#pragma once
// model.h

#include "contactmodel/src/contactmodelmechanical.h"

#ifdef rrHertz_LIB
#  define rrHertz_EXPORT EXPORT_TAG
#elif defined(NO_MODEL_IMPORT)
#  define rrHertz_EXPORT
#else
#  define rrHertz_EXPORT IMPORT_TAG
#endif

namespace cmodelsxd {
	using namespace itasca;

	class ContactModelrrHertz : public ContactModelMechanical {
	public:
		// Constructor: Set default values for contact model properties.
		rrHertz_EXPORT ContactModelrrHertz();
		// Destructor, called when contact is deleted: free allocated memory, etc.
		rrHertz_EXPORT virtual ~ContactModelrrHertz();
		// Contact model name (used as keyword for commands and FISH).
		virtual QString  getName() const { return "rrHertz"; }
		// The index provides a quick way to determine the type of contact model.
		// Each type of contact model in PFC must have a unique index; this is assigned
		// by PFC when the contact model is loaded. This index should be set to -1
		virtual void     setIndex(int i) { index_ = i; }
		virtual int      getIndex() const { return index_; }
		// Contact model version number (e.g., MyModel05_1). The version number can be
		// accessed during the save-restore operation (within the archive method,
		// testing {stream.getRestoreVersion() == getMinorVersion()} to allow for 
		// future modifications to the contact model data structure.
		virtual uint     getMinorVersion() const;
		// Copy the state information to a newly created contact model.
		// Provide access to state information, for use by copy method.
		virtual void     copy(const ContactModel *c);
		// Provide save-restore capability for the state information.
		virtual void     archive(ArchiveStream &);
		// Enumerator for the properties.
		enum PropertyKeys {
			kwHzShear = 1
			, kwHzPoiss
			, kwFric
			, kwHzAlpha
			, kwHzS
			, kwHzSd
			, kwHzF
			, kwDpNRatio
			, kwDpSRatio
			, kwDpMode
			, kwDpF
			, kwDpAlpha
			, kwRGap
			, kwResFric
			, kwResMoment
			, kwResS
			, kwResKr
			, kwUserArea
		};
		// Contact model property names in a comma separated list. The order corresponds with
		// the order of the PropertyKeys enumerator above. One can visualize any of these 
		// properties in PFC automatically. 
		virtual QString  getProperties() const {
			return "hz_shear"
				",hz_poiss"
				",fric"
				",hz_alpha"
				",hz_slip"
				",hz_mode"
				",hz_force"
				",dp_nratio"
				",dp_sratio"
				",dp_mode"
				",dp_force"
				",dp_alpha"
				",rgap"
				",rr_fric"
				",rr_moment"
				",rr_slip"
				",rr_kr"
				",user_area";
		}
		// Enumerator for the energies.
		enum EnergyKeys {
			kwEStrain = 1
			, kwERRStrain
			, kwESlip
			, kwERRSlip
			, kwEDashpot
		};
		// Contact model energy names in a comma separated list. The order corresponds with
		// the order of the EnergyKeys enumerator above. 
		virtual QString  getEnergies() const {
			return "energy-strain"
				",energy-rrstrain"
				",energy-slip"
				",energy-rrslip"
				",energy-dashpot";
		}
		// Returns the value of the energy (base 1 - getEnergy(1) returns the estrain energy).
		virtual double   getEnergy(uint i) const;
		// Returns whether or not each energy is accumulated (base 1 - getEnergyAccumulate(1) 
		// returns wther or not the estrain energy is accumulated which is false).
		virtual bool     getEnergyAccumulate(uint i) const;
		// Set an energy value (base 1 - setEnergy(1) sets the estrain energy).
		virtual void     setEnergy(uint i, const double &d); // Base 1
		// Activate the energy. This is only called if the energy tracking is enabled. 
		virtual void     activateEnergy() { if (energies_) return; energies_ = NEWC(Energies()); }
		// Returns whether or not the energy tracking has been enabled for this contact.
		virtual bool     getEnergyActivated() const { return (energies_ != 0); }

		// Enumerator for contact model related FISH callback events. 
		enum FishCallEvents {
			fActivated = 0
			, fSlipChange
		};
		// Contact model FISH callback event names in a comma separated list. The order corresponds with
		// the order of the FishCallEvents enumerator above. 
		virtual QString  getFishCallEvents() const {
			return
				"contact_activated"
				",slip_change";
		}

		// Return the specified contact model property.
		virtual QVariant getProperty(uint i, const IContact *) const;
		// The return value denotes whether or not the property corresponds to the global
		// or local coordinate system (TRUE: global system, FALSE: local system). The
		// local system is the contact-plane system (nst) defined as follows.
		// If a vector V is expressed in the local system as (Vn, Vs, Vt), then V is
		// expressed in the global system as {Vn*nc + Vs*sc + Vt*tc} where where nc, sc
		// and tc are unit vectors in directions of the nst axes.
		// This is used when rendering contact model properties that are vectors.
		virtual bool     getPropertyGlobal(uint i) const;
		// Set the specified contact model property, ensuring that it is of the correct type
		// and within the correct range --- if not, then throw an exception.
		// The return value denotes whether or not the update has affected the timestep
		// computation (by having modified the translational or rotational tangent stiffnesses).
		// If true is returned, then the timestep will be recomputed.
		virtual bool     setProperty(uint i, const QVariant &v, IContact *);
		// The return value denotes whether or not the property is read-only
		// (TRUE: read-only, FALSE: read-write).
		virtual bool     getPropertyReadOnly(uint i) const;

		// The return value denotes whether or not the property is inheritable
		// (TRUE: inheritable, FALSE: not inheritable). Inheritance is provided by
		// the endPropertyUpdated method.
		virtual bool     supportsInheritance(uint i) const;
		// Return whether or not inheritance is enabled for the specified property.
		virtual bool     getInheritance(uint i) const { assert(i < 32); quint32 mask = to<quint32>(1 << i);  return (inheritanceField_ & mask) ? true : false; }
		// Set the inheritance flag for the specified property.
		virtual void     setInheritance(uint i, bool b) { assert(i < 32); quint32 mask = to<quint32>(1 << i);  if (b) inheritanceField_ |= mask;  else inheritanceField_ &= ~mask; }

		// Prepare for entry into ForceDispLaw. The validate function is called when:
		// (1) the contact is created, (2) a property of the contact that returns a true via
		// the setProperty method has been modified and (3) when a set of cycles is executed
		// via the {cycle N} command.
		// Return value indicates contact activity (TRUE: active, FALSE: inactive).
		virtual bool    validate(ContactModelMechanicalState *state, const double &timestep);
		// The endPropertyUpdated method is called whenever a surface property (with a name
		// that matches an inheritable contact model property name) of one of the contacting
		// pieces is modified. This allows the contact model to update its associated
		// properties. The return value denotes whether or not the update has affected
		// the time step computation (by having modified the translational or rotational
		// tangent stiffnesses). If true is returned, then the time step will be recomputed.  
		virtual bool    endPropertyUpdated(const QString &name, const IContactMechanical *c);
		// The forceDisplacementLaw function is called during each cycle. Given the relative
		// motion of the two contacting pieces (via
		//   state->relativeTranslationalIncrement_ (Ddn, Ddss, Ddst)
		//   state->relativeAngularIncrement_       (Dtt, Dtbs, Dtbt)
		//     Ddn  : relative normal-displacement increment, Ddn > 0 is opening
		//     Ddss : relative  shear-displacement increment (s-axis component)
		//     Ddst : relative  shear-displacement increment (t-axis component)
		//     Dtt  : relative twist-rotation increment
		//     Dtbs : relative  bend-rotation increment (s-axis component)
		//     Dtbt : relative  bend-rotation increment (t-axis component)
		//       The relative displacement and rotation increments:
		//         Dd = Ddn*nc + Ddss*sc + Ddst*tc
		//         Dt = Dtt*nc + Dtbs*sc + Dtbt*tc
		//       where nc, sc and tc are unit vectors in direc. of the nst axes, respectively.
		//       [see {Table 1: Contact State Variables} in PFC Model Components:
		//       Contacts and Contact Models: Contact Resolution]
		// ) and the contact properties, this function must update the contact force and
		// moment (via state->force_, state->momentOn1_, state->momentOn2_).
		//   The force_ is acting on piece 2, and is expressed in the local coordinate system
		//   (defined in getPropertyGlobal) such that the first component positive denotes
		//   compression. If we define the moment acting on piece 2 by Mc, and Mc is expressed
		//   in the local coordinate system (defined in getPropertyGlobal), then we must set
		//   {state->momentOn1_ = -Mc}, {state->momentOn2_ = Mc} and call
		//   state->getMechanicalContact()->updateResultingTorquesLocal(...) after having set
		//   force_.
		// The return value indicates the contact activity status (TRUE: active, FALSE:
		// inactive) during the next cycle.
		// Additional information:
		//   * If state->activated() is true, then the contact has just become active (it was
		//     inactive during the previous time step).
		//   * Fully elastic behavior is enforced during the SOLVE ELASTIC command by having
		//     the forceDispLaw handle the case of {state->canFail_ == true}.
		virtual bool    forceDisplacementLaw(ContactModelMechanicalState *state, const double &timestep);
		// The getEffectiveXStiffness functions return the translational and rotational
		// tangent stiffnesses used to compute a stable time step. When a contact is sliding,
		// the translational tangent shear stiffness is zero (but this stiffness reduction
		// is typically ignored when computing a stable time step). If the contact model
		// includes a dashpot, then the translational stiffnesses must be increased (see
		// Potyondy (2009)).
		//   [Potyondy, D. “Stiffness Matrix at a Contact Between Two Clumps, ?Itasca
		//   Consulting Group, Inc., Minneapolis, MN, Technical Memorandum ICG6863-L,
		//   December 7, 2009.]
		virtual DVect2  getEffectiveTranslationalStiffness() const { return effectiveTranslationalStiffness_; }
		virtual DAVect  getEffectiveRotationalStiffness() const { return effectiveRotationalStiffness_; }

		// Return a new instance of the contact model. This is used in the CMAT
		// when a new contact is created. 
		virtual ContactModelrrHertz *clone() const { return NEWC(ContactModelrrHertz()); }
		// The getActivityDistance function is called by the contact-resolution logic when
		// the CMAT is modified. Return value is the activity distance used by the
		// checkActivity function. For the hill model, this is zero, because we
		// provide a moisture gap via the proximity of the CMAT followed by CLEAN command.
		virtual double              getActivityDistance() const { return rgap_; }
		// The isOKToDelete function is called by the contact-resolution logic when...
		// Return value indicates whether or not the contact may be deleted.
		// If TRUE, then the contact may be deleted when it is inactive.
		// If FALSE, then the contact may not be deleted (under any condition).
		virtual bool                isOKToDelete() const { return !isBonded(); }
		// Zero the forces and moments stored in the contact model. This function is called
		// when the contact becomes inactive.
		virtual void                resetForcesAndMoments() {
			hz_F(DVect(0.0));
			dp_F(DVect(0.0));
			res_M(DAVect(0.0));
			if (energies_) {
				energies_->estrain_ = 0.0;
				energies_->errstrain_ = 0.0;
			}
		}
		virtual void                setForce(const DVect &v, IContact *c);
		virtual void                setArea(const double &d) { throw Exception("The setArea method cannot be used with the Hertz contact model."); }

		// The checkActivity function is called by the contact-resolution logic when...
		// Return value indicates contact activity (TRUE: active, FALSE: inactive).
		// A contact with the hill model is active if it is wet, or if the contact gap is
		// less than or equal to zero.
		virtual bool     checkActivity(const double &gap) { return  gap <= rgap_; }

		// Returns the sliding state (FALSE is returned if not implemented).
		virtual bool     isSliding() const { return hz_slip_; }
		// Returns the bonding state (FALSE is returned if not implemented).
		virtual bool     isBonded() const { return false; }

		// Both of these methods are called only for contacts with facets where the wall 
		// resolution scheme is set the full. In such cases one might wish to propagate 
		// contact state information (e.g., shear force) from one active contact to another. 
		// See the Faceted Wall section in the documentation. 
		virtual void     propagateStateInformation(IContactModelMechanical* oldCm, const CAxes &oldSystem = CAxes(), const CAxes &newSystem = CAxes());
		virtual void     setNonForcePropsFrom(IContactModel *oldCM);

		// Return the total force that the contact model holds.
		virtual DVect    getForce(const IContactMechanical *) const;

		// Return the total moment on 1 that the contact model holds
		virtual DAVect   getMomentOn1(const IContactMechanical *) const;

		//Return the total moment on 1 that the contact model holds
		virtual DAVect   getMomentOn2(const IContactMechanical *) const;

		// Methods to get and set properties. 
		const double & hz_shear() const { return hz_shear_; }
		void           hz_shear(const double &d) { hz_shear_ = d; }
		const double & hz_poiss() const { return hz_poiss_; }
		void           hz_poiss(const double &d) { hz_poiss_ = d; }
		const double & fric() const { return fric_; }
		void           fric(const double &d) { fric_ = d; }
		uint           hz_mode() const { return hz_mode_; }
		void           hz_mode(uint i) { hz_mode_ = i; }
		const double & hz_alpha() const { return hz_alpha_; }
		void           hz_alpha(const double &d) { hz_alpha_ = d; }
		const DVect &  hz_F() const { return hz_F_; }
		void           hz_F(const DVect &f) { hz_F_ = f; }
		bool           hz_S() const { return hz_slip_; }
		void           hz_S(bool b) { hz_slip_ = b; }
		const double & hn() const { return hn_; }
		void           hn(const double &d) { hn_ = d; }
		const double & hs() const { return hs_; }
		void           hs(const double &d) { hs_ = d; }
		const double & rgap() const { return rgap_; }
		void           rgap(const double &d) { rgap_ = d; }


		bool     hasDamping() const { return dpProps_ ? true : false; }
		double   dp_nratio() const { return (hasDamping() ? (dpProps_->dp_nratio_) : 0.0); }
		void     dp_nratio(const double &d) { if (!hasDamping()) return; dpProps_->dp_nratio_ = d; }
		double   dp_sratio() const { return hasDamping() ? dpProps_->dp_sratio_ : 0.0; }
		void     dp_sratio(const double &d) { if (!hasDamping()) return; dpProps_->dp_sratio_ = d; }
		int      dp_mode() const { return hasDamping() ? dpProps_->dp_mode_ : -1; }
		void     dp_mode(int i) { if (!hasDamping()) return; dpProps_->dp_mode_ = i; }
		DVect    dp_F() const { return hasDamping() ? dpProps_->dp_F_ : DVect(0.0); }
		void     dp_F(const DVect &f) { if (!hasDamping()) return; dpProps_->dp_F_ = f; }
		double   dp_alpha() const { return hasDamping() ? dpProps_->dp_alpha_ : 0.0; }
		void     dp_alpha(const double &d) { if (!hasDamping()) return; dpProps_->dp_alpha_ = d; }

		bool    hasEnergies() const { return energies_ ? true : false; }
		double  estrain() const { return hasEnergies() ? energies_->estrain_ : 0.0; }
		void    estrain(const double &d) { if (!hasEnergies()) return; energies_->estrain_ = d; }
		double  errstrain() const { return hasEnergies() ? energies_->errstrain_ : 0.0; }
		void    errstrain(const double &d) { if (!hasEnergies()) return; energies_->errstrain_ = d; }
		double  eslip() const { return hasEnergies() ? energies_->eslip_ : 0.0; }
		void    eslip(const double &d) { if (!hasEnergies()) return; energies_->eslip_ = d; }
		double  errslip() const { return hasEnergies() ? energies_->errslip_ : 0.0; }
		void    errslip(const double &d) { if (!hasEnergies()) return; energies_->errslip_ = d; }
		double  edashpot() const { return hasEnergies() ? energies_->edashpot_ : 0.0; }
		void    edashpot(const double &d) { if (!hasEnergies()) return; energies_->edashpot_ = d; }

		uint inheritanceField() const { return inheritanceField_; }
		void inheritanceField(uint i) { inheritanceField_ = i; }

		const DVect2 & effectiveTranslationalStiffness()  const { return effectiveTranslationalStiffness_; }
		void           effectiveTranslationalStiffness(const DVect2 &v) { effectiveTranslationalStiffness_ = v; }
		const DAVect & effectiveRotationalStiffness()  const { return effectiveRotationalStiffness_; }
		void           effectiveRotationalStiffness(const DAVect &v) { effectiveRotationalStiffness_ = v; }

		// Rolling resistance methods
		const double & res_fric() const { return res_fric_; }
		void           res_fric(const double &d) { res_fric_ = d; }
		const DAVect & res_M() const { return res_M_; }
		void           res_M(const DAVect &f) { res_M_ = f; }
		bool           res_S() const { return res_S_; }
		void           res_S(bool b) { res_S_ = b; }
		const double & kr() const { return kr_; }
		void           kr(const double &d) { kr_ = d; }
		const double & fr() const { return fr_; }
		void           fr(const double &d) { fr_ = d; }
	private:
		// Index - used internally by PFC. Should be set to -1 in the cpp file. 
		static int index_;

		// Structure to store the energies. 
		struct Energies {
			Energies() : estrain_(0.0), errstrain_(0.0), eslip_(0.0), errslip_(0.0), edashpot_(0.0) {}
			double estrain_;   // elastic energy stored in linear group 
			double errstrain_; // elastic energy stored in rolling resistance group
			double eslip_;     // work dissipated by friction 
			double errslip_;   // work dissipated by rolling resistance friction 
			double edashpot_;  // work dissipated by dashpots
		};

		// Structure to store dashpot quantities. 
		struct dpProps {
			dpProps() : dp_nratio_(0.0), dp_sratio_(0.0), dp_mode_(0), dp_F_(DVect(0.0)), dp_alpha_(0.0) {}
			double dp_nratio_;     // normal viscous critical damping ratio
			double dp_sratio_;     // shear  viscous critical damping ratio
			int    dp_mode_;      // for viscous mode (0-4) 0 = dashpots, 1 = tensile limit, 2 = shear limit, 3 = limit both
			DVect  dp_F_;  // Force in the dashpots
			double dp_alpha_;
		};

		bool   updateStiffCoef(const IContactMechanical *con);
		bool   updateEndStiffCoef(const IContactMechanical *con);
		bool   updateEndFric(const IContactMechanical *con);
		bool   updateResFric(const IContactMechanical *con);

		void   updateEffectiveStiffness(ContactModelMechanicalState *state);

		void   setDampCoefficients(const ContactModelMechanicalState &state, double *vcn, double *vcs);

		// Contact model inheritance fields.
		quint32 inheritanceField_;

		// Effective translational stiffness.
		DVect2  effectiveTranslationalStiffness_;
		DAVect  effectiveRotationalStiffness_;

		// rrHertz model properties
		double      hz_shear_;  // Shear modulus
		double      hz_poiss_;  // Poisson ratio
		double      fric_;      // Coulomb friction coefficient
		double      hz_alpha_;  // Exponent
		bool        hz_slip_;      // the current sliding state
		uint        hz_mode_;     // specifies down-scaling of the shear force when normal unloading occurs 
		DVect       hz_F_;      // Force carried in the hertz model
		double      rgap_;      // Reference gap 
		dpProps *   dpProps_;   // The viscous properties

		double      hn_;                           // normal stiffness coefficient
		double      hs_;                           // shear stiffness coefficient       
		// rolling resistance properties
		double res_fric_;       // rolling friction coefficient
		DAVect res_M_;          // moment (bending only)         
		bool   res_S_;          // The current rolling resistance slip state
		double kr_;             // bending rotational stiffness (read-only, calculated internaly) 
		double fr_;             // rolling friction coefficient (rbar*res_fric_) (calculated internaly, not a property) 
		double userArea_;   // Area as specified by the user 
		Energies *   energies_; // The energies


	};
} // namespace cmodelsxd
// EoF
