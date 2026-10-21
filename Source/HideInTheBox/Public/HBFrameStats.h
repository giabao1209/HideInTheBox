#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

/** Wall-clock intervals between consecutive observed engine frames, not GPU timings. */
namespace HBPerformance
{
class FFrameStats
{
public:
    static constexpr std::size_t Capacity = 240;
    void Reset()
    {
        this->Count = 0;
        this->Next = 0;
        this->TotalSeconds = 0.0;
        this->bHasPrevious = false;
    }
    void Observe(std::uint64_t Frame, double Now, bool bPaused)
    {
        if (bPaused || !std::isfinite(Now))
        {
            this->bHasPrevious = false;
            return;
        }
        if (this->bHasPrevious && Frame == this->PreviousFrame)
        {
            return; // Multiple draws in one engine frame must not inflate FPS.
        }
        if (this->bHasPrevious && Frame > this->PreviousFrame && Frame - this->PreviousFrame == 1)
        {
            const double Seconds = Now - this->PreviousTime;
            if (std::isfinite(Seconds) && Seconds > 0.0)
            {
                this->Push(Seconds);
            }
        }
        this->PreviousFrame = Frame;
        this->PreviousTime = Now;
        this->bHasPrevious = true;
    }
    std::size_t Num() const { return this->Count; }
    double SecondsAt(std::size_t Index) const
    {
        if (Index >= this->Count)
        {
            return 0.0;
        }
        return this->Samples[(this->Next + Capacity - this->Count + Index) % Capacity];
    }
    double CurrentMilliseconds() const { return this->Count ? this->SecondsAt(this->Count - 1) * 1000.0 : 0.0; }
    double CurrentFPS() const { return this->Count ? 1.0 / this->SecondsAt(this->Count - 1) : 0.0; }
    double AverageFPS() const { return this->TotalSeconds > 0.0 ? this->Count / this->TotalSeconds : 0.0; }
    double AverageMilliseconds() const { return this->Count ? this->TotalSeconds * 1000.0 / this->Count : 0.0; }
    double WindowSeconds() const { return this->TotalSeconds; }
    double WorstMilliseconds() const
    {
        double Worst = 0.0;
        for (std::size_t Index = 0; Index < this->Count; ++Index)
        {
            const double Sample = this->SecondsAt(Index) * 1000.0;
            if (Sample > Worst)
            {
                Worst = Sample;
            }
        }
        return Worst;
    }
private:
    void Push(double Seconds)
    {
        if (this->Count == Capacity)
        {
            this->TotalSeconds -= this->Samples[this->Next];
        }
        else
        {
            ++this->Count;
        }
        this->Samples[this->Next] = Seconds;
        this->TotalSeconds += Seconds;
        this->Next = (this->Next + 1) % Capacity;
    }
    std::array<double, Capacity> Samples{};
    std::size_t Count = 0;
    std::size_t Next = 0;
    double TotalSeconds = 0.0;
    std::uint64_t PreviousFrame = 0;
    double PreviousTime = 0.0;
    bool bHasPrevious = false;
};
}
