#pragma once

// Reference resolution from the game (1280x640)
inline constexpr double REF_WIDTH = 1280.0;
inline constexpr double REF_HEIGHT = 640.0;

// Table anchor points in reference coordinates
inline constexpr double REF_TABLE_LEFT = 207.0;
inline constexpr double REF_TABLE_RIGHT = 1072.0;
inline constexpr double REF_TABLE_TOP = 171.0;
inline constexpr double REF_TABLE_BOTTOM = 584.0;

// Calculated table dimensions in reference space
inline constexpr double REF_TABLE_WIDTH = REF_TABLE_RIGHT - REF_TABLE_LEFT;  // 865.0

inline double TABLE_LEFT = 0.0;
inline double TABLE_TOP = 0.0;
inline double TABLE_RIGHT = 0.0;
inline double TABLE_BOTTOM = 0.0;
inline double TABLE_SCALE = 1.0;   // X scale (kept for compatibility)
inline double TABLE_SCALE_X = 1.0; // Pixels per world-unit, horizontal
inline double TABLE_SCALE_Y = 1.0; // Pixels per world-unit, vertical (separate: screen is NOT exactly 2:1)

ImVec2 WorldToScreen(Vec2d worldPos) {
    double positionX = worldPos.x + TABLE_HALF_WIDTH;
    double positionY = -(worldPos.y + TABLE_HALF_HEIGHT);
    double scrX = TABLE_LEFT + positionX * TABLE_SCALE;
    double scrY = TABLE_BOTTOM + positionY * TABLE_SCALE; // Uniform scale: game physics uses square world-units
    return ImVec2(scrX, scrY);
}

void UpdateScreenTable() {
    // Calculate scale based on screen height (matches Java implementation)
    double heightScale = Height / REF_HEIGHT;
    
    // Calculate horizontal offset for centering
    double scaledRefWidth = heightScale * REF_WIDTH;
    double offsetX = (Width - scaledRefWidth) / 2.0;
    
    // Apply scaling and centering to anchor points
    TABLE_LEFT = offsetX + (heightScale * REF_TABLE_LEFT);
    TABLE_RIGHT = offsetX + (heightScale * REF_TABLE_RIGHT);
    TABLE_TOP = heightScale * REF_TABLE_TOP;
    TABLE_BOTTOM = heightScale * REF_TABLE_BOTTOM;
    
    // Single uniform scale: the game physics coordinate system is square (1 unit = same pixels X and Y).
    // The apparent pixel-space aspect difference is already encoded in the REF anchor constants.
    TABLE_SCALE = (TABLE_RIGHT - TABLE_LEFT) / TABLE_WIDTH;
    TABLE_SCALE_X = TABLE_SCALE; // aliases for any legacy references
    TABLE_SCALE_Y = TABLE_SCALE;
}