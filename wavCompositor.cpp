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
#include <cstring>

//wavCompositorExtended

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


static void resizeAudioBuffer(float*& buffer1, float*& buffer2, int& bufferSize, const int& newBufferSize)
{
    std::printf("Resizing...\n");

    if (bufferSize > newBufferSize)
    {
        std::cerr << "Error: Current size (" << bufferSize
            << ") is larger than new size (" << newBufferSize << ")\n";
        return;
    }

    // 分配新缓冲区
    float* newBuffer1 = new float[newBufferSize];
    float* newBuffer2 = new float[newBufferSize];

    // 复制旧数据
    std::memcpy(newBuffer1, buffer1, bufferSize * sizeof(float));
    std::memcpy(newBuffer2, buffer2, bufferSize * sizeof(float));

    // 将新增部分清零
    std::memset(newBuffer1 + bufferSize, 0, (newBufferSize - bufferSize) * sizeof(float));
    std::memset(newBuffer2 + bufferSize, 0, (newBufferSize - bufferSize) * sizeof(float));

    // 释放旧缓冲区
    delete[] buffer1;
    delete[] buffer2;

    // 更新指针（通过引用，外部也会更新）
    buffer1 = newBuffer1;
    buffer2 = newBuffer2;

    // 更新大小
    bufferSize = newBufferSize;

}
struct AudioClip {
    std::string filename="";
    float startTime=0.0f;
    float volume=.0f;
};

float safeStof(const std::string& str) {
    try {
        return std::stof(str);
    }
    catch (...) {
        throw std::runtime_error("Invalid number: " + str);
    }
}

std::vector<struct AudioClip> parseInputFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::printf("Cannot open file: %s\n", filename.c_str());
        throw std::runtime_error("Cannot open file");
    }

    std::vector<std::string> tokens;
    std::string line;
    std::printf("reading:%s\n", filename.c_str());


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

    std::vector<struct AudioClip> clips;
    std::printf("File name\tVolume|Start time\n");
    for (size_t i = 0; i < tokens.size(); i += 3) {
        struct AudioClip clip;
        clip.filename = tokens[i];
        clip.startTime = safeStof(tokens[i + 1]);
        clip.volume = safeStof(tokens[i + 2]);

        if (clip.startTime < 0) {
            clip.startTime = 0;
        }
        if (clip.volume < 0) {
            clip.volume = 0;
        }

        std::printf("%s\t%.2f|%.2f\n", clip.filename.c_str(), clip.volume, clip.startTime);
        clips.push_back(clip);
    }

    return clips;
}

inline static void showHelp(char* argv0)
{
    std::cerr << "Usage: " << argv0 << " <input.txt> [-o output.wav] [-s <sample rate> default:44100]\n";
    std::printf("Input file must contain groups of 3: <wavfile> <starttime> <volume>\n.");
}
int main(int argc, char* argv[]) {
#ifdef _WIN32
    system("chcp 65001 > nul");
#endif
    std::cout << "wavCompositorExtended2.0\n";
    int sampleRate = 44100;
    if (argc < 2) {
        showHelp(argv[0]);
        return -1;
    }

    std::string txtFile = argv[1];
    std::string outputFile = "result.wav";

    for (int i = 1; i < argc - 1; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h") {
            showHelp(argv[0]);
            return 0;
        }
        else if (arg == "-o") {
            if (i + 1 >= argc)
            {
                std::cerr << "Where is your output file?!\n";
                return -1;

            }
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

    //try {
        std::vector<struct AudioClip> clips = parseInputFile(txtFile);
        if (clips.empty()) {
            std::cerr << "No valid clips found.\n";
            return 1;
        }

        size_t maxEndSample = 0;
        std::vector<AudioFile<float>> audioFiles;
        audioFiles.reserve(clips.size());

        std::cout << "Loading and resampling audio files...\n";
        int maxDuration = 0;
        for (const AudioClip& clip : clips)
        {
            maxDuration = std::max(maxDuration, static_cast<int>(clip.startTime));
        }
        //std::vector<std::vector<float>> buffer(2, std::vector<float>(maxDuration*sampleRate,.0f));
        int bufferSize = maxDuration * sampleRate;
        //int bufferSize = 24;
        float* buffer1 = new float[bufferSize];
        float* buffer2 = new float[bufferSize];
        float* buffer[] = { buffer1,buffer2 };
        std::memset(buffer1, 0, bufferSize * sizeof(float));
        std::memset(buffer2, 0, bufferSize * sizeof(float));

        for (const AudioClip& clip : clips)
        {
            AudioFile<float> audio;
            if (!audio.load(clip.filename))
            {
                std::printf("Failed to load %s\n", clip.filename.c_str());
                continue;
            }
            const int originalSampleRate = audio.getSampleRate();

          
            if (originalSampleRate != sampleRate)
            {
                std::printf("resampling.\n");
                for (int ch = 0; ch < audio.getNumChannels(); ++ch)
                {
                    resampleAudio(audio.samples[ch], originalSampleRate, sampleRate);
                }
                audio.setSampleRate(sampleRate); // 更新元数据
            }
            const float startTime = clip.startTime;
            float endTime = clip.startTime + audio.getLengthInSeconds();
            int startSampleinBuffer = static_cast<int>(std::round(startTime * sampleRate));
            int endSampleinBuffer = std::floor(endTime * sampleRate);
            std::printf("%s\t%.2fs vol:%.2f|%.2fs->%.2fs\n",clip.filename.c_str(), static_cast<float>(audio.getLengthInSeconds()), clip.volume, startTime, endTime);
            if (endSampleinBuffer > bufferSize)
            {
                int newBufferSize = endSampleinBuffer;
                std::printf("Resize buffer to %d(%d MiB)\n", newBufferSize, static_cast<int>(newBufferSize * sizeof(float) * 2) / 1048576);
   


                resizeAudioBuffer(buffer1, buffer2, bufferSize, newBufferSize);
                buffer[0] = buffer1;
                buffer[1] = buffer2;
            }
            if (audio.getNumChannels() == 1)
            {
                int count = 0;
                for (int i = startSampleinBuffer; i < endSampleinBuffer; ++i, ++count)
                {

                    try
                    {
                        auto temp=audio.samples.at(0).at(count);
                    }
                    catch (const std::out_of_range& e)
                    {
                        std::cerr << " audio out of range\n" << e.what();
                        break;
                    }
                    float sample = audio.samples.at(0).at(count) * clip.volume;
                    while (i > bufferSize - 1)
                    {
                        resizeAudioBuffer(buffer1, buffer2, bufferSize, bufferSize * 2);
                        buffer[0] = buffer1;
                        buffer[1] = buffer2;
                    }
                     buffer[0][i] += sample;
                     buffer[1][i] += sample;

                    
                }

            }
            else
            {
                int count = 0;
                for (int i = startSampleinBuffer; i < endSampleinBuffer; ++i, ++count)
                {
                    try
                    {
                        auto temp=audio.samples.at(0).at(count);
                       temp= audio.samples.at(1).at(count);
                    }
                    catch (const std::out_of_range& e)

                    {
                        std::cerr << " audio out of range\n"<<e.what();
                        break;
                    }
                    while (i > bufferSize - 1)
                    {
   


                        resizeAudioBuffer(buffer1, buffer2, bufferSize, bufferSize * 2);  
                        buffer[0] = buffer1;
                        buffer[1] = buffer2;
                    }
                   buffer[0][i] += audio.samples.at(0).at(count) * clip.volume;
                   buffer[1][i] += audio.samples.at(1).at(count) * clip.volume;
 

                }


                    //assert(count == audio.getNumSamplesPerChannel());
            }


        }

        ;
        // 裁剪末尾静音
        buffer[0] = buffer1;
        buffer[1] = buffer2;
        int lastNonZero = 0;
        for (int i = bufferSize; i > 0; --i) {
            if (buffer[0][i] == 0 && buffer[1][i] == 0) {
                lastNonZero = i;
                break;
            }
        }
        if (lastNonZero > 0) {
            bufferSize = lastNonZero;
            //buffer[0].resize(lastNonZero);
            //buffer[1].resize(lastNonZero);
        }

        // 归一化
        float maxVal = 0.0f;
        for (int i = 0; i < bufferSize;++i) {
                
            maxVal = std::max(maxVal, std::abs(buffer[0][i]));
            maxVal = std::max(maxVal, std::abs(buffer[1][i]));
            }
        if (maxVal > 1.0f) {
            float gain = 1.0f / maxVal;
            for (int i = 0; i < bufferSize; ++i) {
                buffer[0][i] *= gain;
                buffer[1][i] *= gain;   
            }
            std::cout << "Normalized audio (max = " << maxVal << ") -> gain = " << gain << "\n";
        }

        AudioFile<float> result;
        std::vector<std::vector<float>> bufferasVector = { std::vector<float>(buffer[0],buffer[0] +bufferSize),std::vector<float>(buffer[1],buffer[1]+bufferSize) };

        result.setAudioBuffer(bufferasVector);
        result.setSampleRate(sampleRate);
        result.setBitDepth(16);

        if (result.save(outputFile)) {
            std::cout << "Saved to " << outputFile << " ("
                << bufferasVector[0].size() / sampleRate << " seconds)\n";
        }
        else {
            std::cerr << "Failed to save: " << outputFile << "\n";
            return 1;
        }

    //}
    //catch (const std::exception& e) {
    //    std::cerr << "Error: " << e.what() << std::endl;
    //    return 1;
    //}

    return 0;
}