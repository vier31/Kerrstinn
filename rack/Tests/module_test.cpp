// Drives the module's process() with a 24 PPQN clock and compares the output
// with the engine given the same sequence of calls. Run with `make test`.
#include "../src/KerrStinn.cpp"

#include <cmath>
#include <cstdio>

Plugin* pluginInstance = nullptr;

namespace {

constexpr float sampleRate = 48000.f;
constexpr int period = 1000; // 120 BPM: 24000 samples per beat / 24

int failures = 0;

void check(bool ok, const char* what) {
	std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok)
		failures++;
}

void writeWav(const std::string& path, int frames) {
	drwav_data_format fmt{};
	fmt.container = drwav_container_riff;
	fmt.format = DR_WAVE_FORMAT_PCM;
	fmt.channels = 2;
	fmt.sampleRate = 44100;
	fmt.bitsPerSample = 16;
	drwav wav;
	drwav_init_file_write(&wav, path.c_str(), &fmt, nullptr);
	std::vector<int16_t> data((size_t) frames * 2);
	for (int i = 0; i < frames; i++) {
		const double t = (double) i / 44100.0;
		data[(size_t) i * 2] = (int16_t) (12000.0 * std::sin(2.0 * M_PI * (110.0 + 400.0 * t) * t) + 3000.0 * std::sin(i * 0.37));
		data[(size_t) i * 2 + 1] = (int16_t) (9000.0 * std::sin(2.0 * M_PI * 220.0 * t) * ((i / 2205) % 2 ? 1.0 : -0.5));
	}
	drwav_write_pcm_frames(&wav, (drwav_uint64) frames, data.data());
	drwav_uninit(&wav);
}

struct Rig {
	KerrStinn m;
	Module::ProcessArgs args;
	long t = 0;

	Rig() {
		args.sampleRate = sampleRate;
		args.sampleTime = 1.f / sampleRate;
		args.frame = 0;
		// Port::setChannels() is a no-op on unpatched ports, so patch them directly.
		m.inputs[KerrStinn::CLOCK_INPUT].channels = 1;
		m.outputs[KerrStinn::LEFT_OUTPUT].channels = 1;
		m.outputs[KerrStinn::RIGHT_OUTPUT].channels = 1;
		std::lock_guard<std::mutex> lock(m.requestMutex);
		m.added = true;
	}

	// One sample; the clock goes high on every period boundary while running.
	void step(bool clockRunning = true, float trig = 0.f, float reset = 0.f) {
		const bool high = clockRunning && (t % period) < period / 2;
		m.inputs[KerrStinn::CLOCK_INPUT].setVoltage(high ? 10.f : 0.f);
		m.inputs[KerrStinn::TRIG_INPUT].setVoltage(trig);
		m.inputs[KerrStinn::RESET_INPUT].setVoltage(reset);
		m.process(args);
		t++;
	}

	// Waits for the worker to publish a state that differs from 'old'.
	std::shared_ptr<const KerrSnapshot> waitForNew(const KerrSnapshot* old) {
		for (int i = 0; i < 2000; i++) {
			auto s = std::atomic_load(&m.latest);
			if (s.get() != old)
				return s;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		return nullptr;
	}

	// Lets the worker go idle and process() pick up its latest state.
	void settle() {
		std::this_thread::sleep_for(std::chrono::milliseconds(60));
		m.snapCountdown = 0;
	}

	float left() { return m.outputs[KerrStinn::LEFT_OUTPUT].getVoltage(); }
	float right() { return m.outputs[KerrStinn::RIGHT_OUTPUT].getVoltage(); }
};

// The engine on its own, fed the same file.
struct Reference {
	kerr::FloatKerrStinn k;
	kerr::FloatLoop loop;
	std::vector<float> l, r;

	explicit Reference(const std::string& path) {
		decodeFile(path, l, r);
		loop.load(k, l.data(), r.data(), (int) l.size());
	}
};

std::unique_ptr<Reference> referenceLoaded(const std::string& path) {
	auto e = std::make_unique<Reference>(path);
	e->k.loaded();
	return e;
}

bool sameBuffer(const Reference& e, const KerrSnapshot& s) {
	if (e.k.engine.getLength() != s.numFrames)
		return false;
	for (int ch = 0; ch < 2; ch++)
		for (int i = 0; i <= s.numFrames; i++)
			if (e.loop.processed(ch)[i] != s.processed[ch][i])
				return false;
	return true;
}

// Expected output of sample 'n' counted from the first clock pulse, for bars = 1..8.
bool playsBack(Rig& rig, const KerrSnapshot& s, int bars, long samples) {
	const long start = rig.t;
	for (long n = 0; n < samples; n++) {
		rig.step(true, 0.f, n == 0 ? 10.f : 0.f);
		const long since = rig.t - 1 - start; // first pulse at n = 0
		const long pulse = since / period;
		const long into = since % period;
		const int barIndex = (int) ((pulse / pulsesPerBar) % bars);
		const double intoBar = (pulse % pulsesPerBar) * (double) period + into;
		// processReplacing()'s arithmetic, written out independently.
		float pos = (float) (intoBar / (pulsesPerBar * (double) period));
		pos = pos / bars;
		pos += (1.0 / bars) * barIndex;
		if (pos > 1.0)
			pos = 0;
		const long idx = (long) (pos * (float) s.numFrames);
		if (rig.left() != 5.f * s.processed[0][idx] || rig.right() != 5.f * s.processed[1][idx]) {
			std::printf("      mismatch at sample %ld (idx %ld): got %f expected %f, pulse %d since %ld period %f playPos %f\n", n, idx,
				rig.left(), 5.f * s.processed[0][idx], rig.m.pulseCount, rig.m.samplesSinceClock, rig.m.pulsePeriod, rig.m.playPos.load());
			return false;
		}
	}
	return true;
}

// Module::fromJson() order: params, then data. (paramsFromJson needs a running Rack.)
void restoreFrom(KerrStinn& m, json_t* state) {
	json_t* paramsJ = json_object_get(state, "params");
	for (int i = 0; i < KerrStinn::PARAMS_LEN; i++)
		m.params[i].setValue((float) json_real_value(json_array_get(paramsJ, i)));
	m.dataFromJson(json_object_get(state, "data"));
}

} // namespace

int main() {
	const std::string dir = "build/test";
	system::createDirectories(dir);
	const std::string wavPath = system::join(dir, "loop.wav");
	writeWav(wavPath, 44100 * 2);

	{
		Rig rig;
		auto initial = std::atomic_load(&rig.m.latest);
		rig.m.loadLoop(wavPath);
		auto loaded = rig.waitForNew(initial.get());
		auto refPtr = referenceLoaded(wavPath);
		Reference& ref = *refPtr;
		check(loaded && loaded->loaded && sameBuffer(ref, *loaded), "loaded loop matches engine::loadFile + setParameter(666)");
		check(loaded && loaded->fileName == "loop.wav", "file name published");

		rig.settle();
		// Run the clock for a few pulses so the tempo is known, then reset on a downbeat.
		rig.t = 0;
		for (int i = 0; i < 5 * period; i++)
			rig.step();
		check(playsBack(rig, *loaded, 1, 96L * period * 2), "1 bar: two bars of playback follow the 24 PPQN clock sample for sample");

		// Stopping the clock silences the output after two pulse periods.
		bool silent = false;
		for (int i = 0; i < 3 * period; i++) {
			rig.step(false);
			if (i > 2 * period + 1)
				silent = rig.left() == 0.f && rig.right() == 0.f;
		}
		check(silent, "output goes silent when the clock stops");

		// Changing Bars reprocesses like setParameter(4, v).
		rig.m.params[KerrStinn::BARS_PARAM].setValue(0.3f);
		rig.step(false);
		auto barsSnap = rig.waitForNew(loaded.get());
		ref.k.setParameter(kerr::kNumBars, 0.3f);
		check(barsSnap && sameBuffer(ref, *barsSnap), "knob change matches setParameter on the engine");
		rig.settle();
		const int bars = kerr::barsFor(0.3f);
		rig.t = 0; // clock restarts after the stop, keeping the measured tempo
		check(playsBack(rig, *barsSnap, bars, 96L * period * (bars + 1)), "multi-bar playback spans 'bars' bars and wraps");

		// Reset mid-bar returns to the downbeat on the coinciding clock pulse.
		for (int i = 0; i < 37 * period + 300; i++)
			rig.step();
		bool resetOk = true;
		{
			const long resetAt = rig.t + (period - rig.t % period); // next clock edge
			while (rig.t < resetAt)
				rig.step();
			rig.step(true, 0.f, 10.f);
			resetOk = rig.m.pulseCount == 0 && rig.m.playPos.load() < 1e-4f;
		}
		check(resetOk, "reset together with a clock pulse restarts at the downbeat");

		// TRIG with no parameter change re-runs setParameter(666) on top of the last result.
		auto before = std::atomic_load(&rig.m.latest);
		rig.m.inputs[KerrStinn::TRIG_INPUT].channels = 1;
		rig.step(true, 10.f);
		auto trigSnap = rig.waitForNew(before.get());
		ref.k.setParameter(kerr::kRecompute, 0.f);
		check(trigSnap && sameBuffer(ref, *trigSnap), "TRIG re-glitches like setParameter(666)");

		// With TRIG patched, CV is held until the next trigger.
		rig.settle();
		rig.m.inputs[KerrStinn::RANDOM_INPUT].channels = 1;
		rig.m.inputs[KerrStinn::RANDOM_INPUT].setVoltage(3.f);
		before = std::atomic_load(&rig.m.latest);
		for (int i = 0; i < 2000; i++)
			rig.step(true, 0.f);
		std::this_thread::sleep_for(std::chrono::milliseconds(60));
		check(std::atomic_load(&rig.m.latest) == before, "CV is ignored between triggers when TRIG is patched");
		rig.step(true, 10.f);
		auto cvSnap = rig.waitForNew(before.get());
		const float expectedRandom = clamp(kerr::defaultValue(kerr::kRand) + 0.3f, 0.f, 1.f);
		ref.k.setParameter(kerr::kRand, expectedRandom);
		check(cvSnap && sameBuffer(ref, *cvSnap), "trigger samples CV and applies knob + CV/10");

		// Save state for the restore tests.
		rig.settle();
		json_t* state = json_object();
		json_t* paramsJ = json_array();
		for (int i = 0; i < KerrStinn::PARAMS_LEN; i++)
			json_array_append_new(paramsJ, json_real(rig.m.params[i].getValue()));
		json_object_set_new(state, "params", paramsJ);
		json_object_set_new(state, "data", rig.m.dataToJson());
		rig.m.onSave(Module::SaveEvent{}); // no patch: must not throw
		json_t* restoredParams = json_deep_copy(state);

		// Restore from JSON: file loaded, then parameters applied in the original's order.
		{
			Rig restore;
			auto init = std::atomic_load(&restore.m.latest);
			restoreFrom(restore.m, restoredParams);
			auto snap = restore.waitForNew(init.get());

			Reference expect(wavPath);
			float values[kerr::kNumParams];
			for (int i = 0; i < kerr::kNumParams; i++)
				values[i] = restore.m.params[i].getValue();
			expect.k.restore(values);
			check(snap && sameBuffer(expect, *snap), "patch restore matches setChunk -> setProgram");
		}

		// Missing file falls back to the copy in patch storage.
		{
			const std::string storage = system::join(dir, "storage");
			system::createDirectories(storage);
			system::copy(wavPath, system::join(storage, "loop"));
			json_object_set_new(json_object_get(restoredParams, "data"), "loopPath", json_string("/nonexistent/gone.wav"));

			Rig restore;
			{
				std::lock_guard<std::mutex> lock(restore.m.requestMutex);
				restore.m.storageDir = storage;
			}
			auto init = std::atomic_load(&restore.m.latest);
			restoreFrom(restore.m, restoredParams);
			auto snap = restore.waitForNew(init.get());
			check(snap && snap->loaded && restore.m.getLoopPath() == "/nonexistent/gone.wav", "missing file restores from patch storage copy");
		}
		json_decref(state);
		json_decref(restoredParams);
	}

	std::printf("\n%s\n", failures == 0 ? "All tests passed." : "Some tests FAILED.");
	return failures == 0 ? 0 : 1;
}
