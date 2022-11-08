#pragma once
#include <cmath>

constexpr auto M_PI = 3.14159265358979323846f;

class vec2 {
public:
	vec2( ) {
		x = y = 0.f;
	}

	vec2( float fx, float fy ) {
		x = fx;
		y = fy;
	}

	float x, y;

	vec2 operator+( const vec2 &input ) const {
		return vec2 { x + input.x, y + input.y };
	}

	vec2 operator-( const vec2 &input ) const {
		return vec2 { x - input.x, y - input.y };
	}

	vec2 operator+( const int input ) const {
		return vec2 { x + input, y + input };
	}

	vec2 operator-( const int input ) const {
		return vec2 { x - input, y - input };
	}

	vec2 operator/( float input ) const {
		return vec2 { x / input, y / input };
	}

	vec2 operator*( float input ) const {
		return vec2 { x * input, y * input };
	}

	vec2 &operator-=( const vec2 &v ) {
		x -= v.x;
		y -= v.y;
		return *this;
	}

	vec2 &operator/=( float input ) {
		x /= input;
		y /= input;
		return *this;
	}

	vec2 &operator*=( float input ) {
		x *= input;
		y *= input;
		return *this;
	}

	bool operator==( const vec2 &v ) {
		return ( x == v.x ) && ( y == v.y );
	}

	bool operator!=( const vec2 &v ) {
		return ( x != v.x ) || ( y != v.y );
	}

	float length( ) const {
		return std::sqrt( ( x * x ) + ( y * y ) );
	}

	vec2 normalized( ) const {
		return { x / length( ), y / length( ) };
	}

	float dot_product( vec2 input ) const {
		return ( x * input.x ) + ( y * input.y );
	}

	float distance( vec2 input ) const {
		return ( *this - input ).length( );
	}

	bool empty( ) const {
		return x == 0.f && y == 0.f;
	}

	vec2 rotate( const float &f ) {
		float _x, _y;

		float s, c;

		float r = f * M_PI / 180.0f;
		s = sin( r );
		c = cos( r );

		_x = x;
		_y = y;

		float nx = ( _x * c ) - ( _y * s );
		float ny = ( _x * s ) + ( _y * c );
		return { nx, ny };
	}
};