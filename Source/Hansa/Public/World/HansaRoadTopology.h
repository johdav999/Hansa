#pragma once

// Presentation-only contract. No engine, asset, provider or persisted-state dependency.
namespace Hansa::Game::RoadTopology
{
	enum class ETile : unsigned char { Isolated, End, Straight, Corner, TJunction, Crossroads };
	struct FChoice { ETile Tile; unsigned char QuarterTurns; };
	// Bits follow positive Unreal yaw: +X, +Y, -X, -Y. Diagonals never connect.
	constexpr unsigned char PositiveX = 1, PositiveY = 2, NegativeX = 4, NegativeY = 8;
	constexpr unsigned char Mask(bool PX, bool PY, bool NX, bool NY)
	{
		return static_cast<unsigned char>((PX ? PositiveX : 0) | (PY ? PositiveY : 0) |
			(NX ? NegativeX : 0) | (NY ? NegativeY : 0));
	}
	constexpr FChoice Resolve(unsigned char Neighbors)
	{
		// Canonical ports: End +X; Straight +/-X; Corner +X/+Y; T +X/+Y/-X.
		constexpr FChoice Choices[] = {
			{ETile::Isolated,0}, {ETile::End,0}, {ETile::End,1}, {ETile::Corner,0},
			{ETile::End,2}, {ETile::Straight,0}, {ETile::Corner,1}, {ETile::TJunction,0},
			{ETile::End,3}, {ETile::Corner,3}, {ETile::Straight,1}, {ETile::TJunction,3},
			{ETile::Corner,2}, {ETile::TJunction,2}, {ETile::TJunction,1}, {ETile::Crossroads,0}
		};
		return Choices[Neighbors & 15];
	}
	constexpr unsigned char CanonicalPorts(ETile Tile)
	{
		switch (Tile)
		{
		case ETile::End: return 1;
		case ETile::Straight: return 5;
		case ETile::Corner: return 3;
		case ETile::TJunction: return 7;
		case ETile::Crossroads: return 15;
		default: return 0;
		}
	}
	constexpr unsigned char RotatePorts(unsigned char Ports, unsigned char QuarterTurns)
	{
		const unsigned int Shift = QuarterTurns & 3;
		return static_cast<unsigned char>(((Ports << Shift) | (Ports >> (4 - Shift))) & 15);
	}
}
