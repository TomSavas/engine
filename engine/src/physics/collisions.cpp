#include "physics/collisions.h"
#include "scenes/scene.h"

#include <print>
#include <glm/glm.hpp>

glm::vec3 farthestAlongVector(Mesh* mesh, glm::vec3 dir)
{
    float maxDot = glm::dot(glm::vec3(mesh->vertices[0]), dir);
    glm::vec3 maxVertex = mesh->vertices[0];

    for (int i = 1; i < mesh->vertices.size(); ++i)
    {
        float dot = glm::dot(glm::vec3(mesh->vertices[i]), dir);
        if (dot > maxDot)
        {
            maxDot = dot;
            maxVertex = mesh->vertices[i];
        }
    }

    return maxVertex;
}

glm::vec3 support(Mesh* a, Mesh* b, glm::vec3 dir)
{
    // Minkowski diff
    return farthestAlongVector(a, dir) - farthestAlongVector(b, -dir);
}

glm::vec3 tripleCross(glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
    return b * glm::dot(c, a) - a * glm::dot(c, b);
}

bool enclosesOrigin(std::vector<glm::vec3>& simplex, glm::vec3& dir)
{
    switch (simplex.size())
    {
        case 2: 
        {
            glm::vec3 ab = simplex[1] - simplex[0];
            // line a0 is the line from the first vertex to the origin
            glm::vec3 a0 = -simplex[0];

            // use the triple-cross-product to calculate a direction perpendicular
            // to line ab in the direction of the origin
            glm::vec3 tmp = glm::cross(ab, a0);
            dir = tripleCross(tmp, ab, dir);

            break;
        }
        case 3: 
        {
            glm::vec3 ac = simplex[2] - simplex[0];
            glm::vec3 ab = simplex[1] - simplex[0];
            dir = glm::cross(ac, ab);

            // ensure it points toward the origin
            glm::vec3 a0 = -simplex[0];
            if (glm::dot(dir, a0) < 0.f)
            {
                dir *= -1;
            }

            break;
        }
        case 4: 
        {
            // calculate the three edges of interest
            glm::vec3 da = simplex[3] - simplex[0];
            glm::vec3 db = simplex[3] - simplex[1];
            glm::vec3 dc = simplex[3] - simplex[2];

            // and the direction to the origin
            glm::vec3 d0 = -simplex[3];

            // check triangles a-b-d, b-c-d, and c-a-d
            glm::vec3 abdNorm = glm::cross(da, db);
            glm::vec3 bcdNorm = glm::cross(db, dc);
            glm::vec3 cadNorm = glm::cross(dc, da);

            if (glm::dot(abdNorm, d0) >= 0) {
                // the origin is on the outside of triangle a-b-d
                // eliminate c!
                simplex.erase(simplex.begin() + 2);
                dir = abdNorm;
            } else if (glm::dot(bcdNorm, d0) >= 0) {
                // the origin is on the outside of triangle bcd
                // eliminate a!
                simplex.erase(simplex.begin());
                dir = bcdNorm;
            } else if (glm::dot(cadNorm, d0) >= 0) {
                // the origin is on the outside of triangle cad
                // eliminate b!
                simplex.erase(simplex.begin() + 1);
                dir = cadNorm;
            } else {
                // the origin is inside all of the triangles!
                return true;
            }

            break;
        }
    }

    return false;
}

GjkInfo naiveGjk(Model* a, Model* b)
{
    std::vector<glm::vec3> simplex;

    glm::vec3 dir = glm::normalize(glm::vec3(4.f));
    simplex.push_back(support(a->mesh, b->mesh, dir));

    dir *= -1.f;
    int i = 0;
    while (i++ < 10)
    {
        simplex.push_back(support(a->mesh, b->mesh, dir));
                
        if (glm::dot(simplex.back(), dir) <= 0.0 || i > 10)
        {
             return GjkInfo {false};
        }

        if (enclosesOrigin(simplex, dir))
        {
            return GjkInfo {true, simplex, dir};
        }
    }

     return GjkInfo {false};
}

std::pair<std::vector<glm::vec4>, size_t> getFaceNormals(
	const std::vector<glm::vec3>& polytope,
	const std::vector<size_t>& faces)
{
	std::vector<glm::vec4> normals;
	size_t minTriangle = 0;
	float  minDistance = FLT_MAX;

	for (size_t i = 0; i < faces.size(); i += 3) {
		glm::vec3 a = polytope[faces[i    ]];
		glm::vec3 b = polytope[faces[i + 1]];
		glm::vec3 c = polytope[faces[i + 2]];

		glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
		float distance = glm::dot(normal, a);

		if (distance < 0) {
			normal   *= -1;
			distance *= -1;
		}

		normals.emplace_back(normal, distance);

		if (distance < minDistance) {
			minTriangle = i / 3;
			minDistance = distance;
		}
	}

	return { normals, minTriangle };
}

void addIfUniqueEdge(
	std::vector<std::pair<size_t, size_t>>& edges,
	const std::vector<size_t>& faces,
	size_t a,
	size_t b)
{
	auto reverse = std::find(                       //      0--<--3
		edges.begin(),                              //     / \ B /   A: 2-0
		edges.end(),                                //    / A \ /    B: 0-2
		std::make_pair(faces[b], faces[a]) //   1-->--2
	);
 
	if (reverse != edges.end()) {
		edges.erase(reverse);
	}
 
	else {
		edges.emplace_back(faces[a], faces[b]);
	}
}

Collision epa(Model* aModel, Model* bModel, GjkInfo info)
{
    std::vector<glm::vec3> polytope(info.simplex.begin(), info.simplex.end());
	std::vector<size_t> faces = {
		0, 1, 2,
		0, 3, 1,
		0, 2, 3,
		1, 3, 2
	};

	// list: vec4(normal, distance), index: min distance
	auto [normals, minFace] = getFaceNormals(polytope, faces);

    glm::vec3 minNormal;
    float minDistance = FLT_MAX;

    int stuckGuard = 0;
    while (minDistance == FLT_MAX && stuckGuard++ < 10) 
    {
        minNormal = normals[minFace];
        minDistance = normals[minFace].w;

        glm::vec3 supportVec = support(aModel->mesh, bModel->mesh, minNormal);
        float sDistance = glm::dot(minNormal, supportVec);

        if (fabs(sDistance - minDistance) > 0.001f) 
        {
            minDistance = FLT_MAX;
            std::vector<std::pair<size_t, size_t>> uniqueEdges;

            for (size_t i = 0; i < normals.size(); i++) 
            {
                // if (SameDirection(normals[i], support)) 
                // if (glm::dot(glm::vec3(normals[i]), supportVec) > 0.f) 
                if (glm::dot(glm::vec3(normals[i]), supportVec) > glm::dot(glm::vec3(normals[i]), polytope[faces[i * 3]])) 
                {
                    size_t f = i * 3;

                    addIfUniqueEdge(uniqueEdges, faces, f, f + 1);
                    addIfUniqueEdge(uniqueEdges, faces, f + 1, f + 2);
                    addIfUniqueEdge(uniqueEdges, faces, f + 2, f);

                    faces[f + 2] = faces.back();
                    faces.pop_back();
                    faces[f + 1] = faces.back();
                    faces.pop_back();
                    faces[f] = faces.back();
                    faces.pop_back();

                    normals[i] = normals.back(); // pop-erase
                    normals.pop_back();

                    i--;
                }
            }

			std::vector<size_t> newFaces;
			for (auto [edgeIndex1, edgeIndex2] : uniqueEdges) {
				newFaces.push_back(edgeIndex1);
				newFaces.push_back(edgeIndex2);
				newFaces.push_back(polytope.size());
			}
			 
			polytope.push_back(supportVec);

			auto [newNormals, newMinFace] = getFaceNormals(polytope, newFaces);

			float oldMinDistance = FLT_MAX;
			for (size_t i = 0; i < normals.size(); i++) {
				if (normals[i].w < oldMinDistance) {
					oldMinDistance = normals[i].w;
					minFace = i;
				}
			}
 
			if (newNormals.size() == 0 || newNormals[newMinFace].w < oldMinDistance) {
				minFace = newMinFace + normals.size();
			}
 
			faces  .insert(faces  .end(), newFaces  .begin(), newFaces  .end());
			normals.insert(normals.end(), newNormals.begin(), newNormals.end());
        }
    }

	Collision collision;

    if (stuckGuard >= 10)
    {
        collision.a = nullptr;
        collision.b = nullptr;
        return collision;
    }

	// collision.collisionVec = glm::normalize(minNormal) * (minDistance + 0.001f);
	collision.collisionVec = glm::normalize(minNormal) * (minDistance);
	collision.a = aModel;
	collision.b = bModel;
 
	return collision;
}

std::vector<Constraints> generateXpbdCollisionConstraints(std::vector<Collision> collision)
{
     std::vector<Constraints> constraints;   

     return constraints;
}

std::vector<Collision> detectCollisions(std::vector<Model>& models) {
    // NOTE(savas): we assume convex models for now
    
    // broad phase
    // TODO(savas): spatial accel
    std::vector<std::pair<Model*, Model*>> potentialCollisions;
    for (int i = 0; i < models.size(); ++i)
    {
        for (int j = i + 1; j < models.size(); ++j)
        {
            potentialCollisions.emplace_back(&models[i], &models[j]);
        }
    }

    // narrow phase
    std::vector<Collision> collisions;
    for (auto& potentialCollision : potentialCollisions) 
    {
        auto gjkInfo = naiveGjk(potentialCollision.first, potentialCollision.second);

        if (gjkInfo.collides)
        {
            collisions.push_back(epa(potentialCollision.first, potentialCollision.second, gjkInfo));
        }
    }

    return collisions;
}

