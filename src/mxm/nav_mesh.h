#pragma once
#include <common/base.h>
#include <common/vector_math.h>
#include <EASTL/vector.h>
#include <EASTL/hash_map.h>
#include <EASTL/fixed_vector.h>

struct NavMesh
{
	enum : u8 { EDGE_USER = 0x10, EDGE_USER_RAISED = 0x20 };

	struct Face
	{
		i32 startEdge = 0;
		i32 startUserEdge = 0;
		i32 numEdges = 0;
		i32 numUserEdges = 0;
		vec2 boxMin = vec2(0);
		vec2 boxMax = vec2(0);
		f32 zMin = 0;
		f32 zMax = 0;
		vec3 center = vec3(0);
	};

	struct Edge
	{
		i32 a = 0;
		i32 b = 0;
		i32 oppositeEdge = -1;
		i32 oppositeFace = -1;
		u8 flags = 0;
	};

	struct Waypoint
	{
		vec3 pos;
		i32 userEdge;
	};

	typedef eastl::fixed_vector<Waypoint,64,true> Route;

	eastl::vector<vec3> verts;
	eastl::vector<Face> faces;
	eastl::vector<Edge> edges;
	eastl::hash_map<u32, eastl::vector<i32>> grid;
	i32 userEdgeCount = 0;

	bool Valid() const { return !faces.empty(); }
	bool Load(const u8* data, i32 size);
	i32 FaceAt(const vec3& pos) const;
	bool Inside(i32 face, const vec2& p) const;
	vec3 EdgeMid(i32 edge) const;
	bool FindRoute(const vec3& from, const vec3& to, Route& out, const eastl::hash_map<i32, u8>* failedUserEdges, u8 maxFails) const;
};
