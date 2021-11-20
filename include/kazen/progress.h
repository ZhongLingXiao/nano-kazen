#pragma once

#include <kazen/common.h>
#include <kazen/timer.h>

NAMESPACE_BEGIN(kazen)

class Progress {
public:
    /**
     * \brief Construct a new progress reporter.
     * \param label
     *     An identifying name for the operation taking place (e.g. "Rendering")
     * \param ptr
     *     Custom pointer payload to be delivered as part of progress messages
     */
    Progress(const std::string &label, void *payload = nullptr);

    /// Update the progress to \c progress (which should be in the range [0, 1])
    void update(float progress);

private:
    Timer m_timer;
    std::string m_label;
    std::string m_line;
    size_t m_barStart;
    size_t m_barSize;
    size_t m_lastUpdate;
    float m_lastProgress;
    void *m_payload;
};

NAMESPACE_END(kazen)