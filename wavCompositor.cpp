#include "AudioFile.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <cmath>



// ✅ 正确的线性插值重采样
template <typename T>
bool resampleAudio(std::vector<T>& input, int sr, int newsr) {
    static_assert(std::is_floating_point_v<T> || std::is_integral_v<T>, "T must be numeric");
    if (sr == newsr || input.empty() || sr <= 0 || newsr <= 0) {
        return true;
    }

    int oldSize = static_cast<int>(input.size());
    int newSize = static_cast<int>(std::round(static_cast<double>(oldSize) * newsr / sr));

    if (newSize == 0) {
        input.clear();
        return true;
    }

    std::vector<T> output;
    output.reserve(newSize);

    for (int i = 0; i < newSize; ++i) {
        double oldIndex = static_cast<double>(i) * sr / newsr;
        int left = static_cast<int>(std::floor(oldIndex));
        int right = left + 1;

        T sample;

        if (right >= oldSize) {
            sample = input[oldSize - 1];
        }
        else if (left < 0) {
            sample = input[0];
        }
        else {
            double t = oldIndex - left;
            sample = static_cast<T>(input[left] * (1 - t) + input[right] * t);
        }

        output.push_back(sample);
    }

    input = std::move(output);
    return true;
}

struct AudioClip {
    std::string filename;
    float startTime;
    float volume;
};

float safeStof(const std::string& str) {
    try {
        return std::stof(str);
    }
    catch (...) {
        throw std::runtime_error("Invalid number: " + str);
    }
}

std::vector<AudioClip> parseInputFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    std::vector<std::string> tokens;
    std::string line;
    std::cout << "Reading " << filename << "\n";

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }
    }

    if (tokens.size() % 3 != 0) {
        throw std::runtime_error("Input file must contain groups of 3: <wavfile> <starttime> <volume>");
    }

    std::vector<AudioClip> clips;
    for (size_t i = 0; i < tokens.size(); i += 3) {
        AudioClip clip;
        clip.filename = tokens[i];
        clip.startTime = safeStof(tokens[i + 1]);
        clip.volume = safeStof(tokens[i + 2]);

        if (clip.startTime < 0) {
            throw std::runtime_error("Start time cannot be negative: " + std::to_string(clip.startTime));
        }
        if (clip.volume < 0) {
            throw std::runtime_error("Volume cannot be negative: " + std::to_string(clip.volume));
        }

        std::cout << "Clip: " << clip.filename
            << ", Start: " << clip.startTime << "s"
            << ", Volume: " << clip.volume << "\n";
        clips.push_back(clip);
    }

    return clips;
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    system("chcp 65001 > nul");
#endif
    int sampleRate = 44100;
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.txt> [-o <output file name> output.wav] [-s <sample rate> default:44100]\n";
        for (int i = 1; i < argc - 1; ++i) {
            const std::string arg = argv[i];
            if (arg == "-h") {
                std::printf("Input file must contain groups of 3: <wavfile> <starttime> <volume>\n.");
                return 0;
            }
        }
    }

    std::string txtFile = argv[1];
    std::string outputFile = "result.wav";

    for (int i = 1; i < argc - 1; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h") {
            std::cerr << "Usage: " << argv[0] << " <input.txt> [-o output.wav] [-s <sample rate> default:44100]\n";
            std::printf("Input file must contain groups of 3: <wavfile> <starttime> <volume>\n.");
            return 0;
        }
        else if (arg == "-o") {
            outputFile = argv[i + 1];
        }
        else if (arg == "-s") {
            int sr = std::stoi(argv[i + 1]);
            if (sr <= 0 || sr > 384000) {
                std::cerr << "Invalid sample rate: " << sr << ". Must be 1~384000 Hz.\n";
                return 1;
            }
            sampleRate = sr;
        }
    }

    try {
        auto clips = parseInputFile(txtFile);
        if (clips.empty()) {
            std::cerr << "No valid clips found.\n";
            return 1;
        }

        size_t maxEndSample = 0;
        std::vector<AudioFile<float>> audioFiles;
        audioFiles.reserve(clips.size());

        std::cout << "Loading and resampling audio files...\n";

        for (const auto& clip : clips) {
            AudioFile<float> audio;
            if (!audio.load(clip.filename)) {  // ✅ 先加载
                throw std::runtime_error("Failed to load: " + clip.filename);
            }

            int originalSampleRate = audio.getSampleRate();
            std::cout << "Loaded: " << clip.filename
                << " (" << originalSampleRate << " Hz, "
                << audio.getNumChannels() << " ch, "
                << audio.getLengthInSeconds() << " s)\n";

            // ✅ 重采样到目标采样率
            if (originalSampleRate != sampleRate) {

                std::cout << "Resampling " << clip.filename << " from " << originalSampleRate
                    << " to " << sampleRate << " Hz\n";

                for (int ch = 0; ch < audio.getNumChannels(); ++ch) {
                    resampleAudio(audio.samples[ch], originalSampleRate, sampleRate);
                }
                audio.setSampleRate(sampleRate); // 更新元数据
            }

            size_t endSample = static_cast<size_t>((clip.startTime + audio.getLengthInSeconds()) * sampleRate);
            maxEndSample = std::max(maxEndSample, endSample);
            audioFiles.push_back(std::move(audio));
        }

        if (maxEndSample == 0) {
            std::cerr << "No audio data to mix.\n";
            return 1;
        }

        std::vector<std::vector<float>> buffer(2, std::vector<float>(maxEndSample, 0.0f));

        for (size_t i = 0; i < clips.size(); ++i) {
            const auto& clip = clips[i];
            const auto& audio = audioFiles[i];
            size_t startSample = static_cast<size_t>(clip.startTime * sampleRate);
            int numChannels = audio.getNumChannels();
            int numSamples = audio.getNumSamplesPerChannel();

            std::cout << "(" << i << ") Mixing: " << clip.filename
                << " at " << clip.startTime << "s (" << startSample << ")\n";

            for (int ch = 0; ch < 2; ++ch) {
                for (int j = 0; j < numSamples; ++j) {
                    float sample = 0.0f;
                    if (numChannels == 1) {
                        sample = audio.samples[0][j];
                    }
                    else if (ch < numChannels) {
                        sample = audio.samples[ch][j];
                    }
                    if (startSample + j < buffer[ch].size()) {
                        buffer[ch][startSample + j] += sample * clip.volume; // ✅ 直接使用 clip.volume
                    }
                }
            }
        }

        // 裁剪末尾静音
        size_t lastNonZero = 0;
        for (size_t i = buffer[0].size(); i > 0; --i) {
            if (std::abs(buffer[0][i - 1]) > 1e-6f || std::abs(buffer[1][i - 1]) > 1e-6f) {
                lastNonZero = i;
                break;
            }
        }
        if (lastNonZero > 0) {
            buffer[0].resize(lastNonZero);
            buffer[1].resize(lastNonZero);
        }

        // 归一化
        float maxVal = 0.0f;
        for (const auto& channel : buffer) {
            for (float s : channel) {
                maxVal = std::max(maxVal, std::abs(s));
            }
        }
        if (maxVal > 1.0f) {
            float gain = 1.0f / maxVal;
            for (auto& channel : buffer) {
                for (float& s : channel) {
                    s *= gain;
                }
            }
            std::cout << "Normalized audio (max = " << maxVal << ") -> gain = " << gain << "\n";
        }

        AudioFile<float> result;
        result.setAudioBuffer(buffer);
        result.setSampleRate(sampleRate);
        result.setBitDepth(16);

        if (result.save(outputFile)) {
            std::cout << "Saved to " << outputFile << " ("
                << buffer[0].size() / sampleRate << " seconds)\n";
        }
        else {
            std::cerr << "Failed to save: " << outputFile << "\n";
            return 1;
        }

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}