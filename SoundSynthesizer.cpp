


#include <list>
#include <iostream>
#include <algorithm>
using namespace std;

#define FTYPE double
#include "olcNoiseMaker.h"


namespace synth
{
	//////////////////////////////////////////////////////////////////////////////
	// Utilities

	// Converts frequency (Hz) to angular velocity
	FTYPE w(const FTYPE dHertz)
	{
		return dHertz * 2.0 * PI;
	}

	struct instrument_base;

	// A basic note
	struct note
	{
		int id;		// Position in scale
		FTYPE on;	// Time note was activated
		FTYPE off;	// Time note was deactivated
		bool active;
		instrument_base* channel;

		note()
		{
			id = 0;
			on = 0.0;
			off = 0.0;
			active = false;
			channel = nullptr;
		}

		//bool operator==(const note& n1, const note& n2) { return n1.id == n2.id; }
	};

	//////////////////////////////////////////////////////////////////////////////
	// Multi-Function Oscillator
	const int OSC_SINE = 0;
	const int OSC_SQUARE = 1;
	const int OSC_TRIANGLE = 2;
	const int OSC_SAW_ANA = 3;
	const int OSC_SAW_DIG = 4;
	const int OSC_NOISE = 5;

	FTYPE osc(const FTYPE dTime, const FTYPE dHertz, const int nType = OSC_SINE,
		const FTYPE dLFOHertz = 0.0, const FTYPE dLFOAmplitude = 0.0, FTYPE dCustom = 50.0)
	{

		FTYPE dFreq = w(dHertz) * dTime + dLFOAmplitude * dHertz * (sin(w(dLFOHertz) * dTime));

		switch (nType)
		{
		case OSC_SINE: // Sine wave bewteen -1 and +1
			return sin(dFreq);

		case OSC_SQUARE: // Square wave between -1 and +1
			return sin(dFreq) > 0 ? 1.0 : -1.0;

		case OSC_TRIANGLE: // Triangle wave between -1 and +1
			return asin(sin(dFreq)) * (2.0 / PI);

		case OSC_SAW_ANA: // Saw wave (analogue / warm / slow)
		{
			FTYPE dOutput = 0.0;
			for (FTYPE n = 1.0; n < dCustom; n++)
				dOutput += (sin(n * dFreq)) / n;
			return dOutput * (2.0 / PI);
		}

		case OSC_SAW_DIG:
			return (2.0 / PI) * (dHertz * PI * fmod(dTime, 1.0 / dHertz) - (PI / 2.0));

		case OSC_NOISE:
			return 2.0 * ((FTYPE)rand() / (FTYPE)RAND_MAX) - 1.0;

		default:
			return 0.0;
		}
	}

	//////////////////////////////////////////////////////////////////////////////
	// Scale to Frequency conversion

	const int SCALE_DEFAULT = 0;

	FTYPE scale(const int nNoteID, const int nScaleID = SCALE_DEFAULT)
	{
		switch (nScaleID)
		{
		case SCALE_DEFAULT: default:
			return 8 * pow(1.0594630943592952645618252949463, nNoteID);
		}
	}


	//////////////////////////////////////////////////////////////////////////////
	// Envelopes

	struct envelope
	{
		virtual FTYPE amplitude(const FTYPE dTime, const FTYPE dTimeOn, const FTYPE dTimeOff) = 0;
	};

	struct envelope_adsr : public envelope
	{
		FTYPE dAttackTime;
		FTYPE dDecayTime;
		FTYPE dSustainAmplitude;
		FTYPE dReleaseTime;
		FTYPE dStartAmplitude;

		envelope_adsr()
		{
			dAttackTime = 0.1;
			dDecayTime = 0.1;
			dSustainAmplitude = 1.0;
			dReleaseTime = 0.2;
			dStartAmplitude = 1.0;
		}

		virtual FTYPE amplitude(const FTYPE dTime, const FTYPE dTimeOn, const FTYPE dTimeOff)
		{
			FTYPE dAmplitude = 0.0;
			FTYPE dReleaseAmplitude = 0.0;

			if (dTimeOn > dTimeOff) // Note is on
			{
				FTYPE dLifeTime = dTime - dTimeOn;

				if (dLifeTime <= dAttackTime)
					dAmplitude = (dLifeTime / dAttackTime) * dStartAmplitude;

				if (dLifeTime > dAttackTime && dLifeTime <= (dAttackTime + dDecayTime))
					dAmplitude = ((dLifeTime - dAttackTime) / dDecayTime) * (dSustainAmplitude - dStartAmplitude) + dStartAmplitude;

				if (dLifeTime > (dAttackTime + dDecayTime))
					dAmplitude = dSustainAmplitude;
			}
			else // Note is off
			{
				FTYPE dLifeTime = dTimeOff - dTimeOn;

				if (dLifeTime <= dAttackTime)
					dReleaseAmplitude = (dLifeTime / dAttackTime) * dStartAmplitude;

				if (dLifeTime > dAttackTime && dLifeTime <= (dAttackTime + dDecayTime))
					dReleaseAmplitude = ((dLifeTime - dAttackTime) / dDecayTime) * (dSustainAmplitude - dStartAmplitude) + dStartAmplitude;

				if (dLifeTime > (dAttackTime + dDecayTime))
					dReleaseAmplitude = dSustainAmplitude;

				dAmplitude = ((dTime - dTimeOff) / dReleaseTime) * (0.0 - dReleaseAmplitude) + dReleaseAmplitude;
			}

			// Amplitude should not be negative
			if (dAmplitude <= 0.01)
				dAmplitude = 0.0;

			return dAmplitude;
		}
	};

	FTYPE env(const FTYPE dTime, envelope& env, const FTYPE dTimeOn, const FTYPE dTimeOff)
	{
		return env.amplitude(dTime, dTimeOn, dTimeOff);
	}


	struct instrument_base
	{
		FTYPE dVolume;
		synth::envelope_adsr env;
		FTYPE fMaxLifeTime;
		wstring name;
		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool& bNoteFinished) = 0;
	};

	struct instrument_bell : public instrument_base
	{
		instrument_bell()
		{
			env.dAttackTime = 0.01;
			env.dDecayTime = 1.0;
			env.dSustainAmplitude = 0.0;
			env.dReleaseTime = 1.0;
			fMaxLifeTime = 3.0;
			dVolume = 1.0;
			name = L"Bell";
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool& bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if (dAmplitude <= 0.0) bNoteFinished = true;

			FTYPE dSound =
				+1.00 * synth::osc(dTime - n.on, synth::scale(n.id + 12), synth::OSC_SINE, 5.0, 0.001)
				+ 0.50 * synth::osc(dTime - n.on, synth::scale(n.id + 24))
				+ 0.25 * synth::osc(dTime - n.on, synth::scale(n.id + 36));

			return dAmplitude * dSound * dVolume;
		}

	};

	struct instrument_bell8 : public instrument_base
	{
		instrument_bell8()
		{
			env.dAttackTime = 0.01;
			env.dDecayTime = 0.5;
			env.dSustainAmplitude = 0.8;
			env.dReleaseTime = 1.0;
			fMaxLifeTime = 3.0;
			dVolume = 1.0;
			name = L"8-Bit Bell";
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool& bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if (dAmplitude <= 0.0) bNoteFinished = true;

			FTYPE dSound =
				+1.00 * synth::osc(dTime - n.on, synth::scale(n.id), synth::OSC_SQUARE, 5.0, 0.001)
				+ 0.50 * synth::osc(dTime - n.on, synth::scale(n.id + 12))
				+ 0.25 * synth::osc(dTime - n.on, synth::scale(n.id + 24));

			return dAmplitude * dSound * dVolume;
		}

	};

	struct instrument_harmonica : public instrument_base
	{
		instrument_harmonica()
		{
			env.dAttackTime = 0.00;
			env.dDecayTime = 1.0;
			env.dSustainAmplitude = 0.95;
			env.dReleaseTime = 0.1;
			fMaxLifeTime = -1.0;
			name = L"Harmonica";
			dVolume = 0.3;
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool& bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if (dAmplitude <= 0.0) bNoteFinished = true;

			FTYPE dSound =
				+1.0 * synth::osc(n.on - dTime, synth::scale(n.id - 12), synth::OSC_SAW_ANA, 5.0, 0.001, 100)
				+ 1.00 * synth::osc(dTime - n.on, synth::scale(n.id), synth::OSC_SQUARE, 5.0, 0.001)
				+ 0.50 * synth::osc(dTime - n.on, synth::scale(n.id + 12), synth::OSC_SQUARE)
				+ 0.05 * synth::osc(dTime - n.on, synth::scale(n.id + 24), synth::OSC_NOISE);

			return dAmplitude * dSound * dVolume;
		}

	};


	struct instrument_drumkick : public instrument_base
	{
		instrument_drumkick()
		{
			env.dAttackTime = 0.01;
			env.dDecayTime = 0.15;
			env.dSustainAmplitude = 0.0;
			env.dReleaseTime = 0.0;
			fMaxLifeTime = 1.5;
			name = L"Drum Kick";
			dVolume = 1.0;
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool& bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if (fMaxLifeTime > 0.0 && dTime - n.on >= fMaxLifeTime)	bNoteFinished = true;

			FTYPE dSound =
				+0.99 * synth::osc(dTime - n.on, synth::scale(n.id - 36), synth::OSC_SINE, 1.0, 1.0)
				+ 0.01 * synth::osc(dTime - n.on, 0, synth::OSC_NOISE);

			return dAmplitude * dSound * dVolume;
		}

	};

	struct instrument_drumsnare : public instrument_base
	{
		instrument_drumsnare()
		{
			env.dAttackTime = 0.0;
			env.dDecayTime = 0.2;
			env.dSustainAmplitude = 0.0;
			env.dReleaseTime = 0.0;
			fMaxLifeTime = 1.0;
			name = L"Drum Snare";
			dVolume = 1.0;
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool& bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if (fMaxLifeTime > 0.0 && dTime - n.on >= fMaxLifeTime)	bNoteFinished = true;

			FTYPE dSound =
				+0.5 * synth::osc(dTime - n.on, synth::scale(n.id - 24), synth::OSC_SINE, 0.5, 1.0)
				+ 0.5 * synth::osc(dTime - n.on, 0, synth::OSC_NOISE);

			return dAmplitude * dSound * dVolume;
		}

	};


	struct instrument_drumhihat : public instrument_base
	{
		instrument_drumhihat()
		{
			env.dAttackTime = 0.01;
			env.dDecayTime = 0.05;
			env.dSustainAmplitude = 0.0;
			env.dReleaseTime = 0.0;
			fMaxLifeTime = 1.0;
			name = L"Drum HiHat";
			dVolume = 0.5;
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool& bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if (fMaxLifeTime > 0.0 && dTime - n.on >= fMaxLifeTime)	bNoteFinished = true;

			FTYPE dSound =
				+0.1 * synth::osc(dTime - n.on, synth::scale(n.id - 12), synth::OSC_SQUARE, 1.5, 1)
				+ 0.9 * synth::osc(dTime - n.on, 0, synth::OSC_NOISE);

			return dAmplitude * dSound * dVolume;
		}

	};


	struct sequencer
	{
	public:
		struct channel
		{
			instrument_base* instrument;
			wstring sBeat;
		};

	public:
		sequencer(float tempo = 120.0f, int beats = 4, int subbeats = 4)
		{
			nBeats = beats;
			nSubBeats = subbeats;
			fTempo = tempo;
			fBeatTime = (60.0f / fTempo) / (float)nSubBeats;
			nCurrentBeat = 0;
			nTotalBeats = nSubBeats * nBeats;
			fAccumulate = 0;
		}


		int Update(FTYPE fElapsedTime)
		{
			vecNotes.clear();

			fAccumulate += fElapsedTime;
			while (fAccumulate >= fBeatTime)
			{
				fAccumulate -= fBeatTime;
				nCurrentBeat++;

				if (nCurrentBeat >= nTotalBeats)
					nCurrentBeat = 0;

				int c = 0;
				for (auto v : vecChannel)
				{
					if (v.sBeat[nCurrentBeat] == L'X')
					{
						note n;
						n.channel = vecChannel[c].instrument;
						n.active = true;
						n.id = 64;
						vecNotes.push_back(n);
					}
					c++;
				}
			}



			return vecNotes.size();
		}

		void AddInstrument(instrument_base* inst)
		{
			channel c;
			c.instrument = inst;
			vecChannel.push_back(c);
		}

	public:
		int nBeats;
		int nSubBeats;
		FTYPE fTempo;
		FTYPE fBeatTime;
		FTYPE fAccumulate;
		int nCurrentBeat;
		int nTotalBeats;

	public:
		vector<channel> vecChannel;
		vector<note> vecNotes;


	private:

	};

}

vector<synth::note> vecNotes;
mutex muxNotes;
synth::instrument_bell instBell;
synth::instrument_harmonica instHarm;
synth::instrument_drumkick instKick;
synth::instrument_drumsnare instSnare;
synth::instrument_drumhihat instHiHat;

typedef bool(*lambda)(synth::note const& item);
template<class T>
void safe_remove(T& v, lambda f)
{
	auto n = v.begin();
	while (n != v.end())
		if (!f(*n))
			n = v.erase(n);
		else
			++n;
}

// Function used by olcNoiseMaker to generate sound waves
// Returns amplitude (-1.0 to +1.0) as a function of time
FTYPE MakeNoise(int nChannel, FTYPE dTime)
{
	unique_lock<mutex> lm(muxNotes);
	FTYPE dMixedOutput = 0.0;

	// Iterate through all active notes, and mix together
	for (auto& n : vecNotes)
	{
		bool bNoteFinished = false;
		FTYPE dSound = 0;

		// Get sample for this note by using the correct instrument and envelope
		if (n.channel != nullptr)
			dSound = n.channel->sound(dTime, n, bNoteFinished);

		// Mix into output
		dMixedOutput += dSound;

		if (bNoteFinished) // Flag note to be removed
			n.active = false;
	}
	// Woah! Modern C++ Overload!!! Remove notes which are now inactive
	safe_remove<vector<synth::note>>(vecNotes, [](synth::note const& item) { return item.active; });
	return dMixedOutput * 0.2;
}

int main()
{



	// Get all sound hardware
	vector<wstring> devices = olcNoiseMaker<short>::Enumerate();

	// Create sound machine!!
	olcNoiseMaker<short> sound(devices[0], 44100, 1, 8, 256);

	// Link noise function with sound machine
	sound.SetUserFunction(MakeNoise);

	// Create Screen Buffer
	wchar_t* screen = new wchar_t[80 * 30];
	HANDLE hConsole = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
	SetConsoleActiveScreenBuffer(hConsole);
	DWORD dwBytesWritten = 0;

	// Lambda function to draw into the character array conveniently
	auto draw = [&screen](int x, int y, wstring s)
	{
		for (size_t i = 0; i < s.size(); i++)
			screen[y * 80 + x + i] = s[i];
	};

	auto clock_old_time = chrono::high_resolution_clock::now();
	auto clock_real_time = chrono::high_resolution_clock::now();
	double dElapsedTime = 0.0;
	double dWallTime = 0.0;

	// Establish Sequencer
	synth::sequencer seq(90.0);
	seq.AddInstrument(&instKick);
	seq.AddInstrument(&instSnare);
	seq.AddInstrument(&instHiHat);

	seq.vecChannel.at(0).sBeat = L"X...X...X..X.X..";  //L"X...X...X..X.X..";
	seq.vecChannel.at(1).sBeat = L"..X...X...X...X.";  //L"..X...X...X...X."
	seq.vecChannel.at(2).sBeat = L"X.X.X.X.X.X.X.XX";  //L"X.X.X.X.X.X.X.XX"

	while (1)
	{
		// --- SOUND STUFF ---

		// Update Timings =======================================================================================
		clock_real_time = chrono::high_resolution_clock::now();
		auto time_last_loop = clock_real_time - clock_old_time;
		clock_old_time = clock_real_time;
		dElapsedTime = chrono::duration<FTYPE>(time_last_loop).count();
		dWallTime += dElapsedTime;
		FTYPE dTimeNow = sound.GetTime();

		// Sequencer (generates notes, note offs applied by note lifespan) ======================================
		int newNotes = seq.Update(dElapsedTime);
		muxNotes.lock();
		for (int a = 0; a < newNotes; a++)
		{
			seq.vecNotes[a].on = dTimeNow;
			vecNotes.emplace_back(seq.vecNotes[a]);
		}
		muxNotes.unlock();

		// Keyboard (generates and removes notes depending on key state) ========================================
		for (int k = 0; k < 16; k++)
		{
			short nKeyState = GetAsyncKeyState((unsigned char)("ZSXCFVGBNJMK\xbcL\xbe\xbf"[k]));

			// Check if note already exists in currently playing notes
			muxNotes.lock();
			auto noteFound = find_if(vecNotes.begin(), vecNotes.end(), [&k](synth::note const& item) { return item.id == k + 64 && item.channel == &instHarm; });
			if (noteFound == vecNotes.end())
			{
				// Note not found in vector
				if (nKeyState & 0x8000)
				{
					// Key has been pressed so create a new note
					synth::note n;
					n.id = k + 64;
					n.on = dTimeNow;
					n.active = true;
					n.channel = &instHarm;

					// Add note to vector
					vecNotes.emplace_back(n);
				}
			}
			else
			{
				// Note exists in vector
				if (nKeyState & 0x8000)
				{
					// Key is still held, so do nothing
					if (noteFound->off > noteFound->on)
					{
						// Key has been pressed again during release phase
						noteFound->on = dTimeNow;
						noteFound->active = true;
					}
				}
				else
				{
					// Key has been released, so switch off
					if (noteFound->off < noteFound->on)
						noteFound->off = dTimeNow;
				}
			}
			muxNotes.unlock();
		}

		// --- VISUAL STUFF ---

		// Clear Background
		for (int i = 0; i < 80 * 30; i++) screen[i] = L' ';

		// Draw Sequencer
		draw(2, 2, L"SEQUENCER:");
		for (int beats = 0; beats < seq.nBeats; beats++)
		{
			draw(beats * seq.nSubBeats + 20, 2, L"O");
			for (int subbeats = 1; subbeats < seq.nSubBeats; subbeats++)
				draw(beats * seq.nSubBeats + subbeats + 20, 2, L".");
		}

		// Draw Sequences
		int n = 0;
		for (auto v : seq.vecChannel)
		{
			draw(2, 3 + n, v.instrument->name);
			draw(20, 3 + n, v.sBeat);
			n++;
		}

		// Draw Beat Cursor
		draw(20 + seq.nCurrentBeat, 1, L"|");

		// Draw Keyboard
		draw(2, 8, L"|   |   |   |   |   | |   |   |   |   | |   | |   |   |   |  ");
		draw(2, 9, L"|   | S |   |   | F | | G |   |   | J | | K | | L |   |   |  ");
		draw(2, 10, L"|   |___|   |   |___| |___|   |   |___| |___| |___|   |   |__");
		draw(2, 11, L"|     |     |     |     |     |     |     |     |     |     |");
		draw(2, 12, L"|  Z  |  X  |  C  |  V  |  B  |  N  |  M  |  ,  |  .  |  /  |");
		draw(2, 13, L"|_____|_____|_____|_____|_____|_____|_____|_____|_____|_____|");

		// Draw Stats
		wstring stats = L"Notes: " + to_wstring(vecNotes.size()) + L" Wall Time: " + to_wstring(dWallTime) + L" CPU Time: " + to_wstring(dTimeNow) + L" Latency: " + to_wstring(dWallTime - dTimeNow);
		draw(2, 15, stats);

		// Update Display
		WriteConsoleOutputCharacter(hConsole, screen, 80 * 30, { 0,0 }, &dwBytesWritten);
	}


	return 0;
}




//// SoundSynthesizer.cpp : This file contains the 'main' function. Program execution begins and ends there.
////
//
//#include <iostream>
//
//using namespace std;
//
//#include "olcNoiseMaker.h"
//
////create a struct for envelope
//struct sEnvelopeADSR {
//
//    //the list of variables to be used
//
//    double dAttackTime;
//    double dDecaytime;
//    double dStartAmplitude;
//    double dSustainAmplitude;
//    double dReleaseTime;
//    double dTriggerOnTime;
//    double dTriggerOffTime;
//    bool bNoteOn;
//
//    //constructor to set default values
//    sEnvelopeADSR() {
//        dAttackTime = 0.100;
//        dDecaytime = 0.01;
//        dStartAmplitude = 1.0;
//        dSustainAmplitude = 0.8;
//        dReleaseTime = 0.200;
//        dTriggerOnTime = 0.0;
//        dTriggerOffTime = 0.0;
//        bNoteOn = false;
//    }
//
//    //set NoteOn 
//    void NoteOn(double dTimeOn)
//    {
//        //set triggeron time to the sound.gettime fn
//        dTriggerOnTime = dTimeOn;
//        bNoteOn = true;
//    }
//    //set note off and fetch off time
//    void NoteOff(double dTimeOn)
//    {
//        //set triggeron time to the sound.gettime fn
//        dTriggerOffTime = dTimeOn;
//        bNoteOn = false;
//    }
//    //returns the amplitide
//    double GetAmplitude(double dTime)
//    {   
//        double dAmplitude = 0.0;
//        double dLifeTime = dTime - dTriggerOnTime;
//
//        //check for ADS when the note is on
//        if (bNoteOn)
//        {
//            //attack
//            if (dLifeTime <= dAttackTime)
//            {
//                dAmplitude = (dLifeTime / dAttackTime) * dStartAmplitude;
//            }
//
//            //decay
//            if (dLifeTime > dAttackTime && dLifeTime <= (dAttackTime + dDecaytime))
//            {
//                dAmplitude = (dLifeTime - dAttackTime) / dDecaytime * (dSustainAmplitude - dStartAmplitude) + dStartAmplitude;
//            }
//            //sustain
//            if (dLifeTime > dAttackTime + dDecaytime)
//            {
//                dAmplitude = dSustainAmplitude;
//            }
//        }
//        else
//        {   
//            //now the key is released
//            dAmplitude = ((dTime - dTriggerOffTime) / dReleaseTime) * (0.0 - dSustainAmplitude) + dSustainAmplitude;
//        }
//        //check if frequency is too low
//        if (dAmplitude <= 0.0001)
//        {
//            dAmplitude = 0.0;
//        }
//        return dAmplitude;
//    }
//};
//
////convert frequency to angular velocity
//double w(double dHertz)
//{
//    return dHertz * 2.0 * PI;
//}
//
////create an ocsillator fn that oscilates between  waves
//
//double osc(double dHertz, double dTime, int ntype)
//{
//    switch (ntype) {
//    case 0: //sine wave
//        return sin(w(dHertz) * dTime);
//    case 1: //square wave
//        return sin(w(dHertz) * dTime) > 0.0 ? 1.0 : -1.0;
//    case 2: //triangle wave
//        return asin(sin(w(dHertz) * dTime) * 2.0 / PI);
//    case 3: //saw wave analogue version, slow and smooth
//        {
//        double dOutput = 0.0;
//        //create a for loop for no. of oscillations
//        for (double n = 1.0; n < 10.0; n++)
//        {
//            dOutput += (sin(n * w(dHertz) * dTime)) / n;
//
//        }
//        return dOutput * (2.0 / PI);
//        }
//    case 4: // saw wave optimized harsh and fast
//        return (2.0 / PI) * (dHertz * PI * fmod(dTime, 1.0 / dHertz) - (PI / 2.0));
//
//
//    default:
//        return 0.0;
//    }
//
//}
////Global variables
//
//atomic<double> dFrequencyOutput = 0.0f;
//
//double dOctaveBaseFrequency = 110.0;
//
//double d12thRootOf2 = pow(2.0, 1.0 / 12.0);
//
//sEnvelopeADSR envelope;
//
//
//// noise fn
//double MakeNoise(double dTime)
//{
//    // giving the sound a periodic form as the sin fn changes in square form
//    double dOutput = envelope.GetAmplitude(dTime) *
//        (
//            +1.0 * osc(dFrequencyOutput * 0.5, dTime, 3)
//            + 1.0 * osc(dFrequencyOutput, dTime, 1)
//            ); //call the osc fn and change the ntype for diff wave forms and mix togethere sin and square
//    //reducing the sound
//    return dOutput * 0.4;
//
//   
//}
//
//
//int main()
//{
//    wcout << "**SoundSynthesizer**\n";
//
//    // get all sound hardwares
//    vector<wstring> devices = olcNoiseMaker<short>::Enumerate();
//
//    //display
//
//    for (auto d : devices) wcout << "Found output devices:" << d << endl;
//
//    // create sound machine
//    olcNoiseMaker<short> sound(devices[0], 44100, 1, 8, 512);
//
//    wcout << endl <<
//        "|   |   |   |   |   | |   |   |   |   | |   | |   |   |   |" << endl <<
//        "|   | S |   |   | F | | G |   |   | J | | K | | L |   |   |" << endl <<
//        "|   |___|   |   |___| |___|   |   |___| |___| |___|   |   |__" << endl <<
//        "|     |     |     |     |     |     |     |     |     |     |" << endl <<
//        "|  Z  |  X  |  C  |  V  |  B  |  N  |  M  |  ,  |  .  |  /  |" << endl <<
//        "|_____|_____|_____|_____|_____|_____|_____|_____|_____|_____|" << endl << endl;
//
//    // link noise fn with machine
//    sound.SetUserFunction(MakeNoise);
//
//    // Sit in loop, capturing keyboard state changes and modify
//       // synthesizer output accordingly
//    int nCurrentKey = -1;
//    bool bKeyPressed = false;
//   
//
//    while (1)
//    {
//        // try to getasyncstate of key and then set dfrequencyoutput
//        bKeyPressed = false;
//
//        for (int k = 0; k < 16; k++)
//        {
//            if (GetAsyncKeyState((unsigned char)("ZSXCFVGBNJMK\xbcL\xbe\xbf"[k])) & 0x8000)
//            {
//                if (nCurrentKey != k)
//                {
//                    dFrequencyOutput = dOctaveBaseFrequency * pow(d12thRootOf2, k);
//                    envelope.NoteOn(sound.GetTime());
//                    wcout << "\rNote On : " << sound.GetTime() << "s " << dFrequencyOutput << "Hz";
//                    nCurrentKey = k;
//                }
//               /* if (nCurrentKey != 2)
//                {
//                    dFrequencyOutput = 12 * 
//                }*/
//
//                bKeyPressed = true;
//            }
//        }
//
//        if (!bKeyPressed)
//        {
//            if (nCurrentKey != -1)
//            {
//                envelope.NoteOff(sound.GetTime());
//                wcout << "\rNote Off: " << sound.GetTime() << "s                        ";
//                nCurrentKey = -1;
//            }
//
//           
//        }
//    }
//
//    return 0;
//}
//
