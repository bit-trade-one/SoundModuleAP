/**********************************************************************

  Audacity: A Digital Audio Editor

  ToneGen.h

  Steve Jolly

  This class implements a tone generator effect.

**********************************************************************/

#ifndef __AUDACITY_EFFECT_TONEGEN__
#define __AUDACITY_EFFECT_TONEGEN__

#include "Generator.h"
#include "../Experimental.h"

#include <wx/dialog.h>

class wxString;
class wxChoice;
class wxTextCtrl;
class NumericTextCtrl;
class ShuttleGui;

#define __UNINITIALIZED__ (-1)

class WaveTrack;


class EffectToneGen : public BlockGenerator {

 public:
   EffectToneGen();
   // A 'Chirp' is a tone that changes in frequency.
   EffectToneGen & EnableForChirps(){mbChirp=true;return *this;};

   virtual wxString GetEffectName() {
      return wxString(mbChirp? wxTRANSLATE("Chirp..."):wxTRANSLATE("Tone..."));
   }

   virtual std::set<wxString> GetEffectCategories() {
      std::set<wxString> result;
      result.insert(wxT("http://lv2plug.in/ns/lv2core#GeneratorPlugin"));
      return result;
   }

   virtual wxString GetEffectIdentifier() {
      return wxString(mbChirp ? wxT("Chirp") : wxT("Tone"));
   }

   virtual wxString GetEffectAction() {
      return wxString(mbChirp? _("Generating Chirp") : _("Generating Tone"));
   }

   // Return true if the effect supports processing via batch chains.
   virtual bool SupportsChains() {
      return false;
   }

   // Useful only after PromptUser values have been set.
   virtual wxString GetEffectDescription();

   virtual bool PromptUser();
   virtual bool TransferParameters( Shuttle & shuttle );

   void GenerateBlock(float *data, const WaveTrack &track, sampleCount block);
   void BeforeGenerate();
   void BeforeTrack(const WaveTrack &track);

 protected:
   virtual bool MakeTone(float *buffer, sampleCount len);

 private:

   bool mbChirp;
   bool mbLogInterpolation;

   double mPositionInCycles;

   // If we made these static variables,
   // Tone and Chirp would share the same parameters.
   int waveform;
   float frequency[2];
   float amplitude[2];
   float logFrequency[2];
   double mCurRate;
   int interpolation;

   // mSample is an external placeholder to remember the last "buffer"
   // position so we use it to reinitialize from where we left
   sampleCount mSample;
 // friendship ...
 friend class ToneGenDialog;

};

// WDR: class declarations

//----------------------------------------------------------------------------
// ToneGenDialog
//----------------------------------------------------------------------------

class ToneGenDialog:public EffectDialog {
 public:
   // constructors and destructors
   ToneGenDialog(EffectToneGen * effect, wxWindow * parent, const wxString & title);

   // WDR: method declarations
   void PopulateOrExchange(ShuttleGui & S);
   bool TransferDataToWindow();
   bool TransferDataFromWindow();

 private:
   void PopulateOrExchangeStandard(ShuttleGui & S);
   void PopulateOrExchangeExtended(ShuttleGui & S);
   void OnTimeCtrlUpdate(wxCommandEvent & event);
   DECLARE_EVENT_TABLE()

 public:
   EffectToneGen *mEffect;
   bool mbChirp;
   wxArrayString *waveforms;
   int waveform;
   double frequency[2];
   double amplitude[2];
   double mDuration;
   bool isSelection;
   int interpolation;
   wxArrayString *interpolations;

 private:
   NumericTextCtrl *mToneDurationT;
};

#endif
