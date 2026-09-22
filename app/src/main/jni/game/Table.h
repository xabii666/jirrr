#pragma once

#include "Types.h"
#include "Foundation.h"
#include "Ball.h"

extern ptr libmain;

struct TableProperties : Class {
    // 0x68 is correct offset for mPockets pointer (Vec2d array of 6 pocket positions)
    // 0x38 was an old incorrect offset - do NOT use it
    FieldImpl<0x68, Vec2d*, false> mPockets;

    TableProperties(ptr instance = 0) : Class(instance), mPockets(instance) {}

    double getPocketRadius() { return 8.0; }
    double getLength() { return 254.0; }
    double getWidth() { return 127.0; }

    // FIXED: Accept ANY valid TableProperties instance (not just narrow table type)
    // The old check 'isInstanceOf("TenByFiveNarrowTableProperties")' was causing
    // GetPocketScreenPos to return {0,0} on most tables, breaking pocket nomination.
    operator bool() { return instance != 0; }
};

#include "FrictionProperties.h"

struct Table : Class {
    Field<0x3b0, TableProperties> mTableProperties;
    Field<0x3c0, FrictionProperties> _frictionProperties;
    Field<0x450, PNSArray<Ball>*> mBalls;
    Field<0x588, Vec4d> mTableCollisionBounds; // x, y, width, height

    Table(ptr instance = 0) : Class(instance), mTableProperties(instance), _frictionProperties(instance), mBalls(instance), mTableCollisionBounds(instance) {}

    operator bool() { return instance && this->isInstanceOf("Table"); }
};
