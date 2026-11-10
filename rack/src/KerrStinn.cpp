// KerrStinn - slicer/glitcher after Johan Larsby's 2007 original.
// Copyright (C) 2007 Johan Larsby, 2026 Jan Schröder.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "plugin.hpp"
#include "KerrFloatLoop.h"
#include "dep/dr_wav.h"

#include <osdialog.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace {

// 24 PPQN clock; the original treated a bar as four quarter notes.
constexpr int ppqn = 24;
constexpr int pulsesPerBar = ppqn * 4;

int derivedFor(int index, float v) {
	switch (index) {
		case kerr::kRand: return kerr::seedFor(v);
		case kerr::kSlices: return kerr::slicesFor(v);
		case kerr::kBisyness: return kerr::bisynessFor(v);
		case kerr::kGenerationMatrix: return kerr::generatorFor(v);
		case kerr::kNumBars: return kerr::barsFor(v);
		default: return v == 0.f ? 0 : 1;
	}
}

bool decodeFile(const std::string& path, std::vector<float>& left, std::vector<float>& right) {
	drwav wav;
	if (!drwav_init_file(&wav, path.c_str(), nullptr))
		return false;

	const drwav_uint64 frames = std::min<drwav_uint64>(wav.totalPCMFrameCount, 1 << 26);
	const size_t channels = wav.channels;
	bool ok = false;

	if (frames > 0 && channels > 0) {
		std::vector<float> interleaved((size_t) frames * channels);
		const size_t read = (size_t) drwav_read_pcm_frames_f32(&wav, frames, interleaved.data());

		// 16-bit files are scaled like convertToFloat(): x / 32767.
		const bool pcm16 = wav.bitsPerSample == 16 && wav.translatedFormatTag == DR_WAVE_FORMAT_PCM;
		const size_t rightOffset = channels > 1 ? 1 : 0;

		left.resize(read);
		right.resize(read);
		for (size_t i = 0; i < read; i++) {
			left[i] = interleaved[i * channels];
			right[i] = interleaved[i * channels + rightOffset];
			if (pcm16) {
				left[i] = kerr::fromPcm16((int) std::lround(left[i] * 32768.f));
				right[i] = kerr::fromPcm16((int) std::lround(right[i] * 32768.f));
			}
		}
		ok = read > 0;
	}

	drwav_uninit(&wav);
	return ok;
}

} // namespace

// Everything process() and the display need from one engine state.
struct KerrSnapshot {
	std::array<std::vector<float>, 2> processed;
	int numFrames = 0;
	int numSlices = 2;
	bool loaded = false;
	bool badFile = false;
	std::string generatorName;
	std::string fileName;
};

struct KerrQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		const float v = getValue();
		if (paramId == kerr::kGenerationMatrix)
			return kerr::generatorName(kerr::generatorFor(v));
		return string::f("%d", derivedFor(paramId, v));
	}
};

struct KerrStinn : Module {
	enum ParamId {
		RANDOM_PARAM,
		SLICES_PARAM,
		BISYNESS_PARAM,
		GENERATOR_PARAM,
		BARS_PARAM,
		ONOFF_PARAM,
		TRIG_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		RANDOM_INPUT,
		SLICES_INPUT,
		BISYNESS_INPUT,
		GENERATOR_INPUT,
		BARS_INPUT,
		CLOCK_INPUT,
		RESET_INPUT,
		TRIG_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		LEFT_OUTPUT,
		RIGHT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ONOFF_LIGHT,
		CLOCK_LIGHT,
		TRIG_LIGHT,
		LIGHTS_LEN
	};

	static constexpr int numCvParams = 5;
	static constexpr int numEngineParams = kerr::kNumParams;

	struct Restore {
		std::string path;
		std::array<float, numEngineParams> values;
		kerr::Rand::Flavour flavour;
	};

	// Worker-thread-only state
	kerr::FloatKerrStinn kerrStinn;
	kerr::FloatLoop loop;
	std::array<float, numEngineParams> applied{};
	bool badFile = false;
	std::vector<std::shared_ptr<const KerrSnapshot>> releasePool;
	int trigSeen = 0;

	// Requests to the worker
	std::thread worker;
	std::mutex requestMutex;
	std::condition_variable requestCv;
	bool exiting = false;
	bool added = false;
	std::optional<std::string> pendingLoad;
	std::optional<Restore> pendingRestore;
	bool pendingRecompute = false;
	std::string storageDir;

	std::array<std::atomic<float>, numEngineParams> targets;
	std::atomic<int> trigCount{0};
	std::atomic<int> randFlavour{(int) kerr::Rand::kMac};

	// Published state
	std::shared_ptr<const KerrSnapshot> latest;
	std::atomic<float> playPos{-1.f};

	std::mutex sourceMutex;
	std::string loopPath;
	std::string storageCopyOf;

	// Audio-thread state
	std::shared_ptr<const KerrSnapshot> audioSnap;
	int snapCountdown = 0;
	dsp::SchmittTrigger clockTrigger, resetTrigger, trigTrigger;
	dsp::BooleanTrigger trigButton;
	dsp::PulseGenerator trigPulse;
	std::array<float, numCvParams> latchedCv{};
	int pulseCount = -1;
	long samplesSinceClock = 0;
	double pulsePeriod = 0.0;
	bool clockSeen = false;
	bool skippedInterval = false;

	KerrStinn() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		static const char* names[] = {"Random", "Slices", "Bisyness", "Generator", "Bars"};
		for (int i = 0; i < numCvParams; i++) {
			configParam<KerrQuantity>(i, 0.f, 1.f, kerr::defaultValue(i), names[i]);
			configInput(i, std::string(names[i]) + " CV (0-10 V adds to knob)");
		}
		configSwitch(ONOFF_PARAM, 0.f, 1.f, kerr::defaultValue(kerr::kOn), "On/Off", {"Off", "On"});
		configButton(TRIG_PARAM, "Re-glitch");

		configInput(CLOCK_INPUT, "Clock (24 PPQN)");
		configInput(RESET_INPUT, "Reset");
		configInput(TRIG_INPUT, "Re-glitch trigger (also samples the CV inputs)");
		configOutput(LEFT_OUTPUT, "Left");
		configOutput(RIGHT_OUTPUT, "Right");
		configLight(CLOCK_LIGHT, "Beat");

		// FloatKerrStinn's constructor has applied the defaults, like
		// AKerrStinn's setProgram (0).
		for (int i = 0; i < numEngineParams; i++)
			targets[i] = applied[i] = kerr::defaultValue(i);

		publish();
		worker = std::thread([this] { run(); });
	}

	~KerrStinn() {
		{
			std::lock_guard<std::mutex> lock(requestMutex);
			exiting = true;
		}
		requestCv.notify_one();
		worker.join();
	}

	//==========================================================================
	void process(const ProcessArgs& args) override {
		if (--snapCountdown <= 0) {
			snapCountdown = 64;
			auto s = std::atomic_load(&latest);
			if (s != audioSnap)
				audioSnap = std::move(s);
		}

		// Re-glitch trigger. With TRIG patched, CV is only taken on a trigger,
		// so each trigger is one deliberate step through the settings.
		const bool trigConnected = inputs[TRIG_INPUT].isConnected();
		bool trig = trigTrigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 1.f);
		trig |= trigButton.process(params[TRIG_PARAM].getValue() > 0.f);

		for (int i = 0; i < numCvParams; i++) {
			const float knob = params[i].getValue();
			if (!inputs[i].isConnected()) {
				latchedCv[i] = 0.f;
				if (targets[i].load() != knob)
					targets[i] = knob;
				continue;
			}

			const float cv = inputs[i].getVoltage() / 10.f;
			if (trigConnected) {
				if (trig)
					latchedCv[i] = cv;
				const float v = clamp(knob + latchedCv[i], 0.f, 1.f);
				if (targets[i].load() != v)
					targets[i] = v;
			}
			else {
				// Every change reprocesses on top of the last result, so free-running
				// CV only counts when it moves to a different step.
				const float v = clamp(knob + cv, 0.f, 1.f);
				if (derivedFor(i, v) != derivedFor(i, targets[i].load()))
					targets[i] = v;
			}
		}
		const float onOff = params[ONOFF_PARAM].getValue();
		if (targets[kerr::kOn].load() != onOff)
			targets[kerr::kOn] = onOff;

		if (trig) {
			trigCount++;
			trigPulse.trigger(0.1f);
		}

		processClock(args);

		const int bars = kerr::barsFor(targets[kerr::kNumBars].load());
		const KerrSnapshot* snap = audioSnap.get();
		const bool running = pulseCount >= 0 && pulsePeriod > 0.0 && samplesSinceClock <= 2.0 * pulsePeriod;

		float left = 0.f, right = 0.f, playhead = -1.f;
		if (running && snap != nullptr && snap->loaded && snap->numFrames > 0 && onOff != 0.f) {
			// AKerrStinn::processReplacing(): nearest-sample playback of the processed
			// loop, stretched over 'bars' bars.
			const double intoBar = (pulseCount % pulsesPerBar) * pulsePeriod + std::min((double) samplesSinceClock, pulsePeriod);
			const float pos = kerr::positionFor(intoBar / (pulsesPerBar * pulsePeriod), pulseCount / pulsesPerBar, bars);

			const int idx = kerr::indexFor(pos, snap->numFrames);
			if (idx >= 0 && idx <= snap->numFrames) {
				left = snap->processed[0][idx];
				right = snap->processed[1][idx];
			}
			playhead = pos;
		}

		outputs[LEFT_OUTPUT].setVoltage(5.f * left);
		outputs[RIGHT_OUTPUT].setVoltage(5.f * right);
		playPos = playhead;

		lights[ONOFF_LIGHT].setBrightness(onOff);
		lights[CLOCK_LIGHT].setBrightnessSmooth(running && pulseCount % ppqn < ppqn / 4 ? 1.f : 0.f, args.sampleTime);
		lights[TRIG_LIGHT].setBrightnessSmooth(trigPulse.process(args.sampleTime) ? 1.f : 0.f, args.sampleTime);
	}

	void processClock(const ProcessArgs& args) {
		samplesSinceClock++;

		if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f)) {
			// The first interval after the clock stopped is not a tempo; keep the old
			// period for it. A second long interval in a row is a slower tempo.
			const bool stalled = pulsePeriod > 0.0 && samplesSinceClock > 2.0 * pulsePeriod;
			if (clockSeen && (!stalled || skippedInterval)) {
				pulsePeriod = (double) samplesSinceClock;
				skippedInterval = false;
			}
			else {
				skippedInterval = true;
			}
			clockSeen = true;
			samplesSinceClock = 0;
			pulseCount++;
		}

		// A reset within 1 ms of a clock pulse means that pulse was the downbeat.
		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f))
			pulseCount = samplesSinceClock <= (long) (0.001f * args.sampleRate) ? 0 : -1;

		if (!inputs[CLOCK_INPUT].isConnected())
			pulseCount = -1;
	}

	//==========================================================================
	void run() {
		std::unique_lock<std::mutex> lock(requestMutex);
		while (!exiting) {
			requestCv.wait_for(lock, std::chrono::milliseconds(4));
			if (exiting)
				break;

			std::optional<Restore> restore;
			std::optional<std::string> load;
			bool recompute = false;
			if (added) {
				restore.swap(pendingRestore);
				load.swap(pendingLoad);
			}
			std::swap(recompute, pendingRecompute);
			const std::string storage = storageDir;
			lock.unlock();

			bool changed = false;
			kerrStinn.engine.random().setFlavour((kerr::Rand::Flavour) randFlavour.load());

			if (restore) {
				// AKerrStinn::setChunk -> setProgram
				kerrStinn.engine.random().setFlavour(restore->flavour);
				if (!restore->path.empty())
					loadIntoEngine(restore->path, storage);
				kerrStinn.restore(restore->values.data());
				applied = restore->values;
				changed = true;
			}

			if (load) {
				// Editor load: engine::loadFile, then setParameter (666) recomputes everything.
				loadIntoEngine(*load, "");
				if (kerrStinn.engine.isAllocated())
					kerrStinn.loaded();
				changed = true;
			}

			bool paramChanged = false;
			for (int i = 0; i < numEngineParams; i++) {
				const float v = targets[i].load();
				if (v != applied[i]) {
					applied[i] = v;
					kerrStinn.setParameter(i, v);
					paramChanged = true;
				}
			}

			const int t = trigCount.load();
			if ((t != trigSeen && !paramChanged) || recompute)
				kerrStinn.setParameter(kerr::kRecompute, 0.f);
			changed |= paramChanged || t != trigSeen || recompute;
			trigSeen = t;

			if (changed)
				publish();

			releasePool.erase(std::remove_if(releasePool.begin(), releasePool.end(),
				[](const std::shared_ptr<const KerrSnapshot>& p) { return p.use_count() == 1; }),
				releasePool.end());

			lock.lock();
		}
	}

	// Falls back to the copy kept in the patch storage when the original file is gone.
	void loadIntoEngine(const std::string& path, const std::string& storage) {
		std::vector<float> left, right;
		bool fromStorage = false;
		bool ok = system::isFile(path) && decodeFile(path, left, right);
		if (!ok && !storage.empty()) {
			ok = decodeFile(system::join(storage, "loop"), left, right);
			fromStorage = ok;
		}

		// GetWavInfo() failing leaves the engine unallocated.
		if (!ok)
			kerrStinn.engine.detach();
		else
			loop.load(kerrStinn, left.data(), right.data(), (int) left.size());
		badFile = !ok;

		std::lock_guard<std::mutex> lock(sourceMutex);
		loopPath = path;
		storageCopyOf = fromStorage ? path : "";
	}

	void publish() {
		auto s = std::make_shared<KerrSnapshot>();
		s->loaded = kerrStinn.engine.isAllocated();
		s->numSlices = kerrStinn.getNumSlices();
		s->badFile = badFile;
		s->generatorName = kerrStinn.engine.getrendertypename();
		{
			std::lock_guard<std::mutex> lock(sourceMutex);
			s->fileName = system::getFilename(loopPath);
		}

		if (s->loaded) {
			s->numFrames = kerrStinn.engine.getLength();
			for (int ch = 0; ch < 2; ch++)
				s->processed[ch].assign(loop.processed(ch), loop.processed(ch) + s->numFrames + 1);
		}

		std::shared_ptr<const KerrSnapshot> published = std::move(s);
		releasePool.push_back(published);
		std::atomic_store(&latest, published);
	}

	//==========================================================================
	void loadLoop(const std::string& path) {
		{
			std::lock_guard<std::mutex> lock(requestMutex);
			pendingLoad = path;
		}
		requestCv.notify_one();
	}

	std::string getLoopPath() {
		std::lock_guard<std::mutex> lock(sourceMutex);
		return loopPath;
	}

	void setRandFlavour(kerr::Rand::Flavour f) {
		randFlavour = (int) f;
		{
			std::lock_guard<std::mutex> lock(requestMutex);
			pendingRecompute = true;
		}
		requestCv.notify_one();
	}

	void onAdd(const AddEvent& e) override {
		std::string dir;
		try {
			dir = getPatchStorageDirectory();
		}
		catch (Exception&) {
		}
		{
			std::lock_guard<std::mutex> lock(requestMutex);
			storageDir = dir;
			added = true;
		}
		requestCv.notify_one();
	}

	void onSave(const SaveEvent& e) override {
		std::string path, copied;
		{
			std::lock_guard<std::mutex> lock(sourceMutex);
			path = loopPath;
			copied = storageCopyOf;
		}
		if (path.empty() || path == copied || !system::isFile(path))
			return;

		try {
			const std::string dir = createPatchStorageDirectory();
			const std::string dest = system::join(dir, "loop");
			system::remove(dest);
			if (system::copy(path, dest)) {
				std::lock_guard<std::mutex> lock(sourceMutex);
				if (loopPath == path)
					storageCopyOf = path;
			}
			std::lock_guard<std::mutex> lock(requestMutex);
			storageDir = dir;
		}
		catch (Exception&) {
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "loopPath", json_string(getLoopPath().c_str()));
		json_object_set_new(rootJ, "rand", json_string(randFlavour.load() == (int) kerr::Rand::kWindows ? "windows" : "mac"));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		Restore r;
		json_t* pathJ = json_object_get(rootJ, "loopPath");
		r.path = pathJ ? json_string_value(pathJ) : "";
		json_t* randJ = json_object_get(rootJ, "rand");
		r.flavour = (randJ && std::string(json_string_value(randJ)) == "windows") ? kerr::Rand::kWindows : kerr::Rand::kMac;

		// Params are restored before dataFromJson(); CV joins in once process() runs.
		for (int i = 0; i < numEngineParams; i++) {
			r.values[i] = params[i].getValue();
			targets[i] = r.values[i];
		}
		randFlavour = (int) r.flavour;

		{
			std::lock_guard<std::mutex> lock(requestMutex);
			pendingRestore = std::move(r);
		}
		requestCv.notify_one();
	}
};

//==============================================================================
namespace {

std::shared_ptr<window::Font> loadFont(const char* name) {
	return APP->window->loadFont(asset::system(std::string("res/fonts/") + name));
}

void drawText(NVGcontext* vg, std::shared_ptr<window::Font> font, float size, NVGcolor color, int align, Vec pos, const std::string& text) {
	if (!font || font->handle < 0)
		return;
	nvgFontFaceId(vg, font->handle);
	nvgFontSize(vg, size);
	nvgFillColor(vg, color);
	nvgTextAlign(vg, align);
	nvgText(vg, pos.x, pos.y, text.c_str(), nullptr);
}

const NVGcolor ink = nvgRGB(0x26, 0x26, 0x26);
const NVGcolor paper = nvgRGB(0xe8, 0xe8, 0xe8);
const NVGcolor lcd = nvgRGB(0x9c, 0xe8, 0xa8);

} // namespace

// Rack's SVG renderer has no text, so the panel lettering is drawn here.
struct KerrLabels : Widget {
	void draw(const DrawArgs& args) override {
		auto title = loadFont("Nunito-Bold.ttf");
		auto mono = loadFont("ShareTechMono-Regular.ttf");
		const int center = NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE;

		drawText(args.vg, title, 22.f, nvgRGBA(0xf4, 0xf4, 0xf4, 0xc8), NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, mm2px(Vec(4.5f, 10.5f)), "Kerrstinn");
		drawText(args.vg, mono, 8.f, nvgRGBA(0xf4, 0xf4, 0xf4, 0xb0), NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE, mm2px(Vec(56.5f, 10.5f)), "shuriken.se");

		struct Label { float x, y; const char* text; };
		static const Label knobLabels[] = {
			{16.f, 42.6f, "RANDOM"}, {45.f, 42.6f, "SLICES"},
			{16.f, 60.6f, "BISYNESS"}, {45.f, 60.6f, "GENERATOR"},
			{16.f, 78.6f, "BARS"}, {38.5f, 78.6f, "ON"}, {51.f, 78.6f, "GLITCH"},
		};
		for (const auto& l : knobLabels)
			drawText(args.vg, title, 8.f, ink, center, mm2px(Vec(l.x, l.y)), l.text);

		static const Label jackLabels[] = {
			{8.5f, 106.6f, "CLK"}, {19.5f, 106.6f, "RST"}, {30.5f, 106.6f, "TRIG"},
			{41.8f, 106.6f, "L"}, {52.8f, 106.6f, "R"},
		};
		for (const auto& l : jackLabels)
			drawText(args.vg, title, 8.f, l.x > 36.f ? paper : ink, center, mm2px(Vec(l.x, l.y)), l.text);

		drawText(args.vg, mono, 7.f, ink, center, mm2px(Vec(19.5f, 121.f)), "24 PPQN");
		drawText(args.vg, title, 7.f, paper, center, mm2px(Vec(47.3f, 121.f)), "OUT");
	}
};

struct KerrDisplay : Widget {
	KerrStinn* module = nullptr;
	const KerrSnapshot* cachedFor = nullptr;
	std::shared_ptr<const KerrSnapshot> snap;
	std::vector<std::pair<float, float>> peaks;

	void updatePeaks(int columns) {
		peaks.assign((size_t) columns, {0.f, 0.f});
		if (!snap || snap->numFrames <= 0)
			return;
		const auto& l = snap->processed[0];
		const auto& r = snap->processed[1];
		for (int c = 0; c < columns; c++) {
			const long a = (long) snap->numFrames * c / columns;
			const long b = std::max(a + 1, (long) snap->numFrames * (c + 1) / columns);
			float lo = 0.f, hi = 0.f;
			for (long i = a; i < b; i++) {
				const float v = 0.5f * (l[i] + r[i]);
				lo = std::min(lo, v);
				hi = std::max(hi, v);
			}
			peaks[c] = {lo, hi};
		}
	}

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3.f);
		nvgFillColor(args.vg, nvgRGB(0x10, 0x14, 0x12));
		nvgFill(args.vg);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;

		auto mono = loadFont("ShareTechMono-Regular.ttf");
		const float pad = 4.f;
		const float top = 14.f, bottom = box.size.y - 16.f;
		const float midY = 0.5f * (top + bottom), halfH = 0.5f * (bottom - top);
		const float w = box.size.x - 2 * pad;

		if (!module) {
			drawText(args.vg, mono, 11.f, lcd, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, Vec(pad, box.size.y - 4.f), "KerrStinn");
			return;
		}

		snap = std::atomic_load(&module->latest);
		const int columns = std::max(1, (int) w);
		if (snap.get() != cachedFor || (int) peaks.size() != columns) {
			updatePeaks(columns);
			cachedFor = snap.get();
		}

		nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);

		if (snap && snap->loaded) {
			// Slice grid
			nvgBeginPath(args.vg);
			for (int i = 1; i < snap->numSlices; i++) {
				const float x = pad + w * i / snap->numSlices;
				nvgMoveTo(args.vg, x, top);
				nvgLineTo(args.vg, x, bottom);
			}
			nvgStrokeColor(args.vg, nvgTransRGBAf(lcd, 0.15f));
			nvgStrokeWidth(args.vg, 0.5f);
			nvgStroke(args.vg);

			// Waveform
			nvgBeginPath(args.vg);
			for (int c = 0; c < columns; c++) {
				const float x = pad + c + 0.5f;
				nvgMoveTo(args.vg, x, midY - clamp(peaks[c].second, -1.f, 1.f) * halfH);
				nvgLineTo(args.vg, x, midY - clamp(peaks[c].first, -1.f, 1.f) * halfH + 0.5f);
			}
			nvgStrokeColor(args.vg, nvgTransRGBAf(lcd, 0.85f));
			nvgStrokeWidth(args.vg, 1.f);
			nvgStroke(args.vg);

			const float pos = module->playPos.load();
			if (pos >= 0.f) {
				nvgBeginPath(args.vg);
				nvgRect(args.vg, pad + pos * w, top - 2.f, 1.f, bottom - top + 4.f);
				nvgFillColor(args.vg, nvgRGB(0xff, 0xff, 0xff));
				nvgFill(args.vg);
			}
		}

		std::string header;
		if (snap && snap->badFile)
			header = "Could not read " + snap->fileName;
		else if (snap && snap->loaded)
			header = snap->fileName;
		else
			header = "Right-click or drop a WAV";
		drawText(args.vg, mono, 9.f, nvgTransRGBAf(lcd, 0.75f), NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, Vec(pad, 10.f), header);

		const int slices = kerr::slicesFor(module->targets[kerr::kSlices].load());
		const int bars = kerr::barsFor(module->targets[kerr::kNumBars].load());
		const std::string gen = snap ? snap->generatorName : "";
		drawText(args.vg, mono, 11.f, lcd, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, Vec(pad, box.size.y - 4.f), gen);
		drawText(args.vg, mono, 9.f, nvgTransRGBAf(lcd, 0.75f), NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE, Vec(box.size.x - pad, box.size.y - 4.f),
			string::f("%d/%dbar", slices, bars));

		nvgResetScissor(args.vg);
	}
};

struct KerrStinnWidget : ModuleWidget {
	KerrStinnWidget(KerrStinn* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/KerrStinn.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		auto* labels = createWidget<KerrLabels>(Vec(0, 0));
		labels->box.size = box.size;
		addChild(labels);

		auto* display = createWidget<KerrDisplay>(mm2px(Vec(4.f, 14.f)));
		display->box.size = mm2px(Vec(52.96f, 22.f));
		display->module = module;
		addChild(display);

		// Knob + CV pairs: Random, Slices / Bisyness, Generator / Bars
		const Vec knobPos[] = {{10.f, 50.f}, {39.f, 50.f}, {10.f, 68.f}, {39.f, 68.f}, {10.f, 86.f}};
		for (int i = 0; i < KerrStinn::numCvParams; i++) {
			addParam(createParamCentered<RoundBlackKnob>(mm2px(knobPos[i]), module, i));
			addInput(createInputCentered<PJ301MPort>(mm2px(knobPos[i].plus(Vec(12.f, 0.f))), module, i));
		}

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(38.5f, 86.f)), module, KerrStinn::ONOFF_PARAM, KerrStinn::ONOFF_LIGHT));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<YellowLight>>>(mm2px(Vec(51.f, 86.f)), module, KerrStinn::TRIG_PARAM, KerrStinn::TRIG_LIGHT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.5f, 114.f)), module, KerrStinn::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(19.5f, 114.f)), module, KerrStinn::RESET_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(30.5f, 114.f)), module, KerrStinn::TRIG_INPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(41.8f, 114.f)), module, KerrStinn::LEFT_OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(52.8f, 114.f)), module, KerrStinn::RIGHT_OUTPUT));

		addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(13.f, 104.6f)), module, KerrStinn::CLOCK_LIGHT));
	}

	void appendContextMenu(Menu* menu) override {
		auto* module = getModule<KerrStinn>();
		if (!module)
			return;

		menu->addChild(new MenuSeparator);

		const std::string path = module->getLoopPath();
		menu->addChild(createMenuItem("Load loop…", path.empty() ? "" : system::getFilename(path), [=]() {
			const std::string dir = path.empty() ? "" : system::getDirectory(path);
			osdialog_filters* filters = osdialog_filters_parse("WAV:wav,wave");
			char* chosen = osdialog_file(OSDIALOG_OPEN, dir.empty() ? nullptr : dir.c_str(), nullptr, filters);
			osdialog_filters_free(filters);
			if (chosen) {
				module->loadLoop(chosen);
				std::free(chosen);
			}
		}));

		menu->addChild(createIndexSubmenuItem("rand()", {"Mac (original)", "Windows"},
			[=]() { return (size_t) module->randFlavour.load(); },
			[=](size_t i) { module->setRandFlavour((kerr::Rand::Flavour) i); }));
	}

	void onPathDrop(const PathDropEvent& e) override {
		auto* module = getModule<KerrStinn>();
		if (module && !e.paths.empty()) {
			module->loadLoop(e.paths.front());
			e.consume(this);
			return;
		}
		ModuleWidget::onPathDrop(e);
	}
};

Model* modelKerrStinn = createModel<KerrStinn, KerrStinnWidget>("KerrStinn");
