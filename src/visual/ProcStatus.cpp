/*
 * ProcStatus.cpp
 *
 * Debug panel showing the game process's /proc/self/status (memory, threads,
 * context switches...) as a label/value table in the top-right corner,
 * refreshed once per second.
 */

#include "common.hpp"

#if ENABLE_VISUALS

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace proc_status
{
static settings::Boolean enabled{ "debug.proc-status", "false" };

// /proc/self/status is rebuilt by the kernel on every open; its values move
// slowly, so read it once a second rather than every frame
constexpr auto REFRESH_INTERVAL = std::chrono::seconds(1);

// VmRSS (resident memory) is the number worth watching: plainer label, MB, highlight
constexpr const char *RESIDENT_LABEL         = "VmRSS";
constexpr const char *RESIDENT_DISPLAY_LABEL = "Physical Mem";

// Values/labels wider than this are cut off with "...", otherwise one long
// field (Mems_allowed, Groups) decides the width of the whole box
constexpr float MAX_LABEL_WIDTH = 190.0f;
constexpr float MAX_VALUE_WIDTH = 200.0f;

struct row
{
    std::string label;
    std::string value;
    float label_width{ 0.0f };
    float value_width{ 0.0f };
    bool highlight{ false };
};

static std::vector<row> rows;
static std::chrono::steady_clock::time_point last_read{};

// Title strip: "PROCESS" on the left, "<name> | pid <pid>" on the right
constexpr const char *TITLE = "PROCESS";
static std::string title_detail;
static float title_width        = 0.0f;
static float title_detail_width = 0.0f;

static float text_width(const std::string &text)
{
    float width = 0.0f, height = 0.0f;
    fonts::esp->stringSize(text, &width, &height);
    // The font may not be built yet on the first frames
    return width > 0.0f ? width : float(text.size()) * 7.0f;
}

// Cut `text` down until it fits `max_width`, marking the cut with "..."
static std::string fit_width(std::string text, float max_width, float &width)
{
    width = text_width(text);
    if (width <= max_width)
        return text;
    while (!text.empty())
    {
        text.pop_back();
        width = text_width(text + "...");
        if (width <= max_width)
            break;
    }
    return text + "...";
}

// "1182844 kB" -> "1155 MB  1182844 kB"; the kernel's kB is KiB, hence 1024.
// Requires the " kB" suffix so fields that merely start with a number
// (SigQ, Groups, Cpus_allowed_list) are left alone.
static std::string with_megabytes(const std::string &kb_value)
{
    constexpr std::string_view suffix = " kB";
    if (kb_value.size() <= suffix.size() || !std::string_view(kb_value).ends_with(suffix))
        return kb_value;
    char *parse_end      = nullptr;
    const long kilobytes = std::strtol(kb_value.c_str(), &parse_end, 10);
    if (parse_end == kb_value.c_str() || kilobytes <= 0)
        return kb_value;
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%ld MB  %s", kilobytes / 1024, kb_value.c_str());
    return buffer;
}

// Node/CPU masks (Mems_allowed, Cpus_allowed) can be hundreds of all-zero
// groups long; drop them so the meaningful last group is what fits on screen
static void strip_zero_groups(std::string &value)
{
    const std::string zero_group = "00000000,";
    size_t end                   = 0;
    while (value.compare(end, zero_group.size(), zero_group) == 0)
        end += zero_group.size();
    value.erase(0, end);
}

static void refresh()
{
    const auto now = std::chrono::steady_clock::now();
    if (!rows.empty() && now - last_read < REFRESH_INTERVAL)
        return;
    last_read = now;
    rows.clear();

    std::ifstream status("/proc/self/status");
    if (!status.is_open())
    {
        row unavailable{ "status", "unavailable" };
        unavailable.label = fit_width(unavailable.label, MAX_LABEL_WIDTH, unavailable.label_width);
        unavailable.value = fit_width(unavailable.value, MAX_VALUE_WIDTH, unavailable.value_width);
        rows.push_back(std::move(unavailable));
        return;
    }

    std::string line;
    while (std::getline(status, line))
    {
        const auto colon = line.find(':');
        if (colon == std::string::npos)
            continue;

        row entry;
        entry.label = line.substr(0, colon);
        // Separator is a varying run of tabs/spaces; some values (Groups,
        // *_allowed_list) have internal spacing worth keeping, so trim the ends only
        std::string value = line.substr(colon + 1);
        const auto first  = value.find_first_not_of(" \t");
        if (first == std::string::npos)
            value.clear();
        else
            value = value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
        strip_zero_groups(value);

        if (entry.label == RESIDENT_LABEL)
        {
            entry.label     = RESIDENT_DISPLAY_LABEL;
            value           = with_megabytes(value);
            entry.highlight = true;
        }
        entry.label = fit_width(entry.label, MAX_LABEL_WIDTH, entry.label_width);
        entry.value = fit_width(value, MAX_VALUE_WIDTH, entry.value_width);
        rows.push_back(std::move(entry));
    }

    std::string name, pid;
    for (const auto &entry : rows)
    {
        if (entry.label == "Name")
            name = entry.value;
        else if (entry.label == "Pid")
            pid = entry.value;
    }
    title_detail       = name.empty() ? std::string{} : name + (pid.empty() ? "" : " | pid " + pid);
    title_width        = text_width(TITLE);
    title_detail_width = title_detail.empty() ? 0.0f : text_width(title_detail);
}

// Text with a 1px shadow so it stays readable over bright map geometry
static void shadowed_string(float x, float y, const rgba_t &color, const std::string &text)
{
    draw::String(int(x + 1.0f), int(y + 1.0f), rgba_t(0.0f, 0.0f, 0.0f, 0.7f), text.c_str(), *fonts::esp);
    draw::String(int(x), int(y), color, text.c_str(), *fonts::esp);
}

static void draw()
{
    if (!enabled || !fonts::esp)
        return;
    refresh();
    if (rows.empty())
        return;

    constexpr float padding         = 6.0f;
    constexpr float screen_margin   = 8.0f;
    constexpr float label_value_gap = 10.0f;
    constexpr float column_gap      = 18.0f;
    constexpr float accent_width    = 2.0f;
    const float line_height         = float(fonts::esp->size + 3);
    const float header_height       = line_height + padding;

    // Same palette as the menu theme
    constexpr rgba_t background   = colors::FromRGBA8(0x1a, 0x1c, 0x1e, 235);
    constexpr rgba_t header_fill  = colors::FromRGBA8(0x23, 0x25, 0x27, 245);
    constexpr rgba_t stripe       = colors::FromRGBA8(0x21, 0x23, 0x25, 235);
    constexpr rgba_t border       = colors::FromRGBA8(0x3c, 0x3e, 0x40, 255);
    constexpr rgba_t accent       = colors::FromRGBA8(0xb5, 0xb8, 0xbb, 255);
    constexpr rgba_t title_color  = colors::FromRGBA8(0xe8, 0xea, 0xec, 255);
    constexpr rgba_t detail_color = colors::FromRGBA8(0x9b, 0x9d, 0x9e, 255);
    constexpr rgba_t label_color  = colors::FromRGBA8(0x9b, 0x9d, 0x9e, 255);
    constexpr rgba_t value_color  = colors::FromRGBA8(0xe8, 0xe8, 0xe9, 255);
    constexpr rgba_t highlight_value_color = colors::FromRGBA8(0xff, 0xff, 0xff, 255);

    // Both gutters are measured so names aren't cut mid-word and values line up
    float widest_label = 0.0f, widest_value = 0.0f;
    for (const auto &entry : rows)
    {
        widest_label = std::max(widest_label, entry.label_width);
        widest_value = std::max(widest_value, entry.value_width);
    }
    const float value_offset = widest_label + label_value_gap;
    const float column_width = value_offset + widest_value;

    // Keep the box to ~60% of the screen height; the ~55 rows flow into extra columns
    const float available_height = std::max(line_height, draw::height * 0.6f - padding * 2.0f);
    const size_t rows_per_column = std::max<size_t>(1, size_t(available_height / line_height));
    const size_t column_count    = (rows.size() + rows_per_column - 1) / rows_per_column;
    const size_t tallest_column  = std::min(rows.size(), rows_per_column);

    const float content_width = column_width * float(column_count) + column_gap * float(column_count - 1);
    const float title_needed  = title_width + (title_detail_width > 0.0f ? column_gap + title_detail_width : 0.0f);
    const float box_width     = std::max(content_width, title_needed) + padding * 2.0f;
    const float body_height   = line_height * float(tallest_column) + padding * 2.0f;
    const float box_height    = header_height + body_height;

    // Top-right corner; the box width follows the values, so derive the left edge every frame
    const float left = std::max(screen_margin, draw::width - box_width - screen_margin);
    const float top  = screen_margin;
    const float body = top + header_height;

    // Panel, title strip and the line under it
    draw::Rectangle(left, top, box_width, box_height, background);
    draw::Rectangle(left, top, box_width, header_height, header_fill);
    draw::Rectangle(left, body - 1.0f, box_width, 1.0f, border);
    shadowed_string(left + padding, top + padding * 0.5f, title_color, TITLE);
    if (!title_detail.empty())
        shadowed_string(left + box_width - padding - title_detail_width, top + padding * 0.5f, detail_color, title_detail);

    // Alternating row stripes, one per row across the full column width
    for (size_t row_number = 1; row_number < tallest_column; row_number += 2)
        draw::Rectangle(left + 1.0f, body + padding + line_height * float(row_number), box_width - 2.0f, line_height, stripe);

    // Dividers between columns
    for (size_t column = 1; column < column_count; ++column)
    {
        const float x = left + padding + float(column) * (column_width + column_gap) - column_gap * 0.5f;
        draw::Rectangle(x, body + padding, 1.0f, body_height - padding * 2.0f, border);
    }

    for (size_t index = 0; index < rows.size(); ++index)
    {
        const auto &entry       = rows[index];
        const size_t column     = index / rows_per_column;
        const size_t row_number = index % rows_per_column;

        const float column_left = left + padding + float(column) * (column_width + column_gap);
        const float y           = body + padding + line_height * float(row_number);
        const float text_y      = y + 1.0f;

        // The watched row gets an accent bar instead of a different colour scheme
        if (entry.highlight)
            draw::Rectangle(column_left - padding + 1.0f, y, accent_width, line_height, accent);

        shadowed_string(column_left, text_y, entry.highlight ? accent : label_color, entry.label);
        shadowed_string(column_left + value_offset, text_y, entry.highlight ? highlight_value_color : value_color, entry.value);
    }

    draw::RectangleOutlined(left, top, box_width, box_height, border, 1.0f);
}

static InitRoutine init([]() { EC::Register(EC::Draw, draw, "proc_status_draw"); });
} // namespace proc_status

#endif
