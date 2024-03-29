#ifndef CORNER_HPP
#define CORNER_HPP

#include <iostream>
#include <cassert>

#include "utils.hpp"

struct Point {
	int _x;
	int _y;

	Point(): _x(0), _y(0) { }
	Point(int p[2]): _x(p[0]), _y(p[1]) { }
	Point(int x, int y): _x(x), _y(y) { }
	void print();
};

struct Corner: Point {
	Corner() : Point() { }
	Corner(int c[2]): Point(c) { }
	Corner(int x, int y) : Point(x, y) { }
};

struct Corners {
	Corner _sw;
	Corner _nw;
	Corner _ne;
	Corner _se;

	Corners() { }
	Corners(Corner sw, Corner nw, Corner ne, Corner se): _sw(sw), _nw(nw), _ne(ne), _se(se) { }
	Corners(int c[4][2]): _sw(c[0]), _nw(c[1]), _ne(c[2]), _se(c[3]) { }
	Corners findDest();
	bool inBounds(Point) const;
	int* xArray();
	int* yArray();
	void print();
};

Corners getCornerInput();

#endif