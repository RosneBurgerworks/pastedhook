/*
 * ProcStatus.cpp
 *
 * Debug panel showing the game process's /proc/self/status (memory, threads,
 * context switches...), refreshed once per second.
 */

#include "common.hpp"

#if ENABLE_VISUALS

#include <algorithm>
#include <chrono>
#include <fstream>
#include <string>
#include <vector>

namespace proc_status
{
static settings::Boolean enabled{ "debug.proc-status", "false" };

static std::vector<std::string> lines;
static std::vector<float> line_widths;
static std::chrono::steady_clock::time_point last_read{};

static void refresh()
{
    const auto now = std::chrono::steady_clock::now();
    if (!lines.empty() && now - last_read < std::chrono::seconds(1))
        return;
    last_read = now;

    lines.clear();
    line_widths.clear();
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line))
    {
        std::replace(line.begin(), line.end(), '\t', ' ');
        // Node/CPU masks (Mems_allowed, Cpus_allowed) can be hundreds of zero
        // groups long; drop the leading all-zero groups
        const std::string zero_group = "00000000,";
        if (auto pos = line.find(": "); pos != std::string::npos)
        {
            const size_t value = pos + 2;
            size_t end         = value;
            while (line.compare(end, zero_group.size(), zero_group) == 0)
                end += zero_group.size();
            line.erase(value, end - value);
        }
        // Never let one line stretch the panel across the screen
        constexpr size_t max_chars = 48;
        if (line.size() > max_chars)
            line = line.substr(0, max_chars - 3) + "...";
        float width = 0.0f, height = 0.0f;
        fonts::esp->stringSize(line, &width, &height);
        // The font may not be built yet on the first frames
        if (width <= 0.0f)
            width = float(line.size()) * 7.0f;
        line_widths.push_back(width);
        lines.push_back(std::move(line));
    }
}

static void draw()
{
    if (!enabled || !fonts::esp)
        return;
    refresh();
    if (lines.empty())
        return;

    constexpr float padding    = 4.0f;
    constexpr float column_gap = 12.0f;
    constexpr float margin     = 8.0f;
    const float line_height    = float(fonts::esp->size + 1);

    // Keep the box to ~45% of the screen height by wrapping into columns
    const size_t rows    = std::max<size_t>(1, size_t(draw::height * 0.45f / line_height));
    const size_t columns = (lines.size() + rows - 1) / rows;
    std::vector<float> column_widths(columns, 0.0f);
    for (size_t i = 0; i < lines.size(); ++i)
        column_widths[i / rows] = std::max(column_widths[i / rows], line_widths[i]);

    float content_width = column_gap * float(columns - 1);
    for (float width : column_widths)
        content_width += width;
    const size_t used_rows = std::min(rows, lines.size());

    const float box_width  = content_width + padding * 2.0f;
    const float box_height = line_height * float(used_rows) + padding * 2.0f;
    const float x          = std::max(margin, draw::width - box_width - margin);
    const float y          = draw::height * 0.25f;

    draw::Rectangle(x, y, box_width, box_height, rgba_t(0.0f, 0.0f, 0.0f, 0.6f));
    float column_x = x + padding;
    for (size_t column = 0; column < columns; ++column)
    {
        for (size_t row = 0; row < rows; ++row)
        {
            const size_t i = column * rows + row;
            if (i >= lines.size())
                break;
            draw::String(int(column_x), int(y + padding + line_height * float(row)), colors::white, lines[i].c_str(), *fonts::esp);
        }
        column_x += column_widths[column] + column_gap;
    }
}

static InitRoutine init([]() { EC::Register(EC::Draw, draw, "proc_status_draw"); });
} // namespace proc_status

#endif
