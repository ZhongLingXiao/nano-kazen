#include <kazen/progress.h>
#include <cmath>


NAMESPACE_BEGIN(kazen)

Progress::Progress(const std::string &label, void *payload): 
    m_label(label), 
    m_line(util::terminalWidth() + 1, ' '),
    m_barStart(label.length() + 3), 
    m_barSize(0), m_payload(payload) {
    //
    m_line[0] = '\r';
    ssize_t barSize = (ssize_t) m_line.length()
        - (ssize_t) m_barStart      /* CR, Label, space, leading bracket */
        - 2                         /* Trailing bracket and space */
        - 22                        /* Max length for ETA string */;

    if (barSize > 0) { /* Is there even space to draw a progress bar? */
        m_barSize = barSize;
        memcpy((char *) m_line.data() + 1, label.data(), label.length());
        m_line[m_barStart - 1] = '[';
        m_line[m_barStart + m_barSize] = ']';
    }

    m_lastUpdate = 0;
    m_lastProgress = -1.f;
}

void Progress::update(float progress) {
    progress = std::min(std::max(progress, 0.f), 1.f);

    if (progress == m_lastProgress)
        return;

    size_t elapsed = m_timer.elapsed();
    if (progress != 1.f && (elapsed - m_lastUpdate < 100 || std::abs(progress - m_lastProgress) < 0.01f))
        return; // Don't refresh too often

    float remaining = elapsed / progress * (1 - progress);
    std::string eta = "(" + util::timeString(elapsed) + ", ETA: " + util::timeString(remaining) + ")";
    if (eta.length() > 22)
        eta.resize(22);

    if (m_barSize > 0) {
        size_t filled = std::min(m_barSize, (size_t) std::round(m_barSize * progress)),
               etaPos = m_barStart + m_barSize + 2;
        memset((char *) m_line.data() + m_barStart, '=', filled);
        memset((char *) m_line.data() + etaPos, ' ', m_line.size() - etaPos - 1);
        memcpy((char *) m_line.data() + etaPos, eta.data(), eta.length());
    }

    // TODO: better print 
    std::cout << m_line << std::flush;

    m_lastUpdate = elapsed;
}

NAMESPACE_END(kazen)