#include "nav_mesh.h"

static const f32 HKX_TO_WORLD = 100.0f;
static const f32 NAV_GRID_CELL = 1000.0f;
static const f32 NAV_FACE_Z_TOLERANCE = 300.0f;
static const f32 NAV_NEAREST_FACE_MAX = 600.0f;
static const f32 HYP_NAV_USER_EDGE_COST = 300.0f;

static u32 BigU32(const u8* p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3]; }
static u32 LittleU32(const u8* p) { u32 v; memmove(&v, p, 4); return v; }
static i32 LittleI32(const u8* p) { i32 v; memmove(&v, p, 4); return v; }
static i16 LittleI16(const u8* p) { i16 v; memmove(&v, p, 2); return v; }
static f32 LittleF32(const u8* p) { f32 v; memmove(&v, p, 4); return v; }

static u32 GridKey(i32 x, i32 y) { return ((u32)(x & 0xffff) << 16) | (u32)(y & 0xffff); }

bool NavMesh::Load(const u8* data, i32 size)
{
	struct Array { u32 type; i32 offset; i32 count; };
	i32 item = -1;
	for(i32 i = 4; i + 4 <= size; i++) {
		if(memcmp(data + i, "ITEM", 4) == 0) { item = i; break; }
	}
	if(item < 0) return false;
	const i32 itemBytes = (i32)(BigU32(data + item - 4) & 0x3FFFFFFFu);
	const i32 itemCount = (itemBytes - 8) / 12;
	ASSERT(item + 4 + itemCount * 12 <= size);
	eastl::fixed_vector<Array,64,true> arrays;
	for(i32 k = 0; k < itemCount; k++) {
		const u8* it = data + item + 4 + 12 * k;
		const u32 t = LittleU32(it);
		if((t >> 28) != 2) continue;
		arrays.push_back(Array{ t & 0xFFFFFFu, 32 + (i32)LittleU32(it + 4), (i32)LittleU32(it + 8) });
	}
	if(arrays.empty()) return false;
	const Array* edgeArray = &arrays[0];
	foreach_const(a, arrays) if(a->count > edgeArray->count) edgeArray = &*a;
	const Array* faceArray = nullptr;
	foreach_const(a, arrays) if(a->offset < edgeArray->offset && edgeArray->offset - a->offset == 16 * a->count) faceArray = &*a;
	const Array* vertArray = nullptr;
	foreach_const(a, arrays) {
		const i32 gap = a->offset - (edgeArray->offset + 20 * edgeArray->count);
		if(gap >= 0 && gap < 16) { vertArray = &*a; break; }
	}
	if(!faceArray || !vertArray) return false;
	ASSERT(vertArray->offset + 16 * vertArray->count <= size);

	verts.resize(vertArray->count);
	for(i32 k = 0; k < vertArray->count; k++) {
		const u8* v = data + vertArray->offset + 16 * k;
		verts[k] = vec3(LittleF32(v), LittleF32(v + 4), LittleF32(v + 8)) * HKX_TO_WORLD;
	}
	edges.resize(edgeArray->count);
	for(i32 k = 0; k < edgeArray->count; k++) {
		const u8* e = data + edgeArray->offset + 20 * k;
		Edge& edge = edges[k];
		edge.a = LittleI32(e);
		edge.b = LittleI32(e + 4);
		edge.oppositeEdge = LittleI32(e + 8);
		edge.oppositeFace = LittleI32(e + 12);
		edge.flags = e[16];
		ASSERT(edge.a >= 0 && edge.a < (i32)verts.size() && edge.b >= 0 && edge.b < (i32)verts.size());
	}
	faces.resize(faceArray->count);
	for(i32 k = 0; k < faceArray->count; k++) {
		const u8* f = data + faceArray->offset + 16 * k;
		Face& face = faces[k];
		face.startEdge = LittleI32(f);
		face.startUserEdge = LittleI32(f + 4);
		face.numEdges = LittleI16(f + 8);
		face.numUserEdges = LittleI16(f + 10);
		ASSERT(face.numEdges >= 3 && face.startEdge >= 0 && face.startEdge + face.numEdges <= (i32)edges.size());
		ASSERT(face.numUserEdges == 0 || (face.startUserEdge >= 0 && face.startUserEdge + face.numUserEdges <= (i32)edges.size()));
		userEdgeCount += face.numUserEdges;
		face.boxMin = vec2(1e30f);
		face.boxMax = vec2(-1e30f);
		face.zMin = 1e30f;
		face.zMax = -1e30f;
		vec3 sum = vec3(0);
		for(i32 j = 0; j < face.numEdges; j++) {
			const vec3& p = verts[edges[face.startEdge + j].a];
			face.boxMin = glm::min(face.boxMin, vec2(p));
			face.boxMax = glm::max(face.boxMax, vec2(p));
			face.zMin = MIN(face.zMin, p.z);
			face.zMax = MAX(face.zMax, p.z);
			sum += p;
		}
		face.center = sum / (f32)face.numEdges;
		const i32 x0 = (i32)floorf(face.boxMin.x / NAV_GRID_CELL), x1 = (i32)floorf(face.boxMax.x / NAV_GRID_CELL);
		const i32 y0 = (i32)floorf(face.boxMin.y / NAV_GRID_CELL), y1 = (i32)floorf(face.boxMax.y / NAV_GRID_CELL);
		for(i32 x = x0; x <= x1; x++) for(i32 y = y0; y <= y1; y++) grid[GridKey(x, y)].push_back(k);
	}
	return true;
}

bool NavMesh::Inside(i32 face, const vec2& p) const
{
	const Face& f = faces[face];
	if(p.x < f.boxMin.x || p.x > f.boxMax.x || p.y < f.boxMin.y || p.y > f.boxMax.y) return false;
	bool inside = false;
	for(i32 i = 0, j = f.numEdges - 1; i < f.numEdges; j = i++) {
		const vec3& a = verts[edges[f.startEdge + i].a];
		const vec3& b = verts[edges[f.startEdge + j].a];
		if((a.y > p.y) != (b.y > p.y) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) inside = !inside;
	}
	return inside;
}

i32 NavMesh::FaceAt(const vec3& pos) const
{
	const auto cell = grid.find(GridKey((i32)floorf(pos.x / NAV_GRID_CELL), (i32)floorf(pos.y / NAV_GRID_CELL)));
	i32 best = -1;
	f32 bestDz = 1e30f;
	if(cell != grid.end()) {
		foreach_const(f, cell->second) {
			const Face& face = faces[*f];
			if(pos.z < face.zMin - NAV_FACE_Z_TOLERANCE || pos.z > face.zMax + NAV_FACE_Z_TOLERANCE || !Inside(*f, vec2(pos))) continue;
			const f32 dz = fabsf(face.center.z - pos.z);
			if(dz < bestDz) { bestDz = dz; best = *f; }
		}
	}
	if(best >= 0) return best;
	f32 bestDist = NAV_NEAREST_FACE_MAX;
	for(i32 f = 0; f < (i32)faces.size(); f++) {
		const vec3 d = faces[f].center - pos;
		const f32 dist = glm::length(vec3(d.x, d.y, d.z * 2.0f));
		if(dist < bestDist) { bestDist = dist; best = f; }
	}
	return best;
}

vec3 NavMesh::EdgeMid(i32 edge) const
{
	return (verts[edges[edge].a] + verts[edges[edge].b]) * 0.5f;
}

bool NavMesh::FindRoute(const vec3& from, const vec3& to, Route& out, const eastl::hash_map<i32, u8>* failedUserEdges, u8 maxFails) const
{
	out.clear();
	const i32 start = FaceAt(from);
	const i32 goal = FaceAt(to);
	if(start < 0 || goal < 0) return false;
	if(start == goal) {
		out.push_back(Waypoint{ to, -1 });
		return true;
	}
	const i32 count = (i32)faces.size();
	eastl::vector<f32> g(count, 1e30f);
	eastl::vector<f32> rank(count, 1e30f);
	eastl::vector<vec3> entry(count, vec3(0));
	eastl::vector<i32> prevFace(count, -1);
	eastl::vector<i32> viaEdge(count, -1);
	eastl::vector<u8> closed(count, 0);
	g[start] = 0;
	rank[start] = glm::length(to - from);
	entry[start] = from;
	for(;;) {
		i32 cur = -1;
		for(i32 f = 0; f < count; f++) if(!closed[f] && rank[f] < 1e29f && (cur < 0 || rank[f] < rank[cur])) cur = f;
		if(cur < 0) return false;
		if(cur == goal) break;
		closed[cur] = 1;
		const Face& face = faces[cur];
		for(i32 pass = 0; pass < 2; pass++) {
			const i32 first = pass == 0 ? face.startEdge : face.startUserEdge;
			const i32 n = pass == 0 ? face.numEdges : face.numUserEdges;
			for(i32 j = 0; j < n; j++) {
				const i32 ei = first + j;
				const Edge& e = edges[ei];
				const i32 next = e.oppositeFace;
				if(next < 0 || next >= count || closed[next]) continue;
				if(pass == 1 && failedUserEdges) {
					const auto fail = failedUserEdges->find(ei);
					if(fail != failedUserEdges->end() && fail->second >= maxFails) continue;
				}
				const vec3 portal = EdgeMid(ei);
				vec3 arrive = portal;
				if(pass == 1) arrive = e.oppositeEdge >= 0 && e.oppositeEdge < (i32)edges.size() ? EdgeMid(e.oppositeEdge) : faces[next].center;
				const f32 cost = g[cur] + glm::length(portal - entry[cur]) + glm::length(arrive - portal) + (pass == 1 ? HYP_NAV_USER_EDGE_COST : 0.0f);
				if(cost >= g[next]) continue;
				g[next] = cost;
				rank[next] = cost + glm::length(to - arrive);
				entry[next] = arrive;
				prevFace[next] = cur;
				viaEdge[next] = pass == 1 ? ei : -(ei + 2);
			}
		}
	}
	eastl::fixed_vector<Waypoint,256,true> reversed;
	reversed.push_back(Waypoint{ to, -1 });
	for(i32 f = goal; f != start && f >= 0; f = prevFace[f]) {
		const i32 via = viaEdge[f];
		if(via >= 0) {
			reversed.push_back(Waypoint{ entry[f], via });
			reversed.push_back(Waypoint{ EdgeMid(via), -1 });
		}
		else reversed.push_back(Waypoint{ EdgeMid(-via - 2), -1 });
	}
	for(i32 k = (i32)reversed.size() - 1; k >= 0 && !out.full(); k--) out.push_back(reversed[k]);
	return true;
}
