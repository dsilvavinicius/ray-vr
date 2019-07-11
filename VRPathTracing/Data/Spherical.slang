import GGXGICommon;
__exported import DodecahedronGlobals;

static const float gEpsilon = 0.0001;
static const float4 N1 = (1.f, 0.f, 0.f, 0.f);
static const float4 N2 = (0.f, 1.f, 0.f, 0.f);

//Initial conditions of a geodesic in the euclidean space
struct euclideanRay
{
    float3 origin;
    float3 dir;
    int border; //boder=1 ==> origin belongs to an edge of the spherical dodecahedron, otherwise border=0
    int hit;
    float dist; //lenght of the polygonal path
    int check; // check =1, if the ray is valid, 0 otherwise
};

//Initial conditions of a geodesic in the unit sphere
struct sphericalRay
{
    float4 origin;
    float4 dir;
    int border; //boder=1 ==> origin belongs to an edge of the spherical dodecahedron, otherwise border=0
    int hit;
    float dist; //lenght of the polygonal path
    int check; // check =1, if the ray is valid, 0 otherwise
};
//*********************************************************************************************************



//*********************************************************************************************************
//
//*********************************************************************************************************
float3 project(float3 v, float3 u)
{
    return v - u * dot(u, v) / dot(u, u);
}
//*********************************************************************************************************



//*********************************************************************************************************
//
//*********************************************************************************************************
float4 project(float4 v, float4 u)
{
    return v - u * dot(u, v) / dot(u, u);
}
//*********************************************************************************************************



//*********************************************************************************************************
//distance in the 3-sphere
//*********************************************************************************************************
float sDistance(float4 p, float4 q)
{
    return acos(dot(p, q) / (gScale * gScale)) * gScale;
}
//*********************************************************************************************************




//*********************************************************************************************************
//Evaluate the geodesic leaving p in the direction of v
//*********************************************************************************************************
float4 evalRay(float4 p, float4 v, float t)
{
    p = normalize(p);
    v = normalize(v);
    return (cos(t / gScale) * p + sin(t / gScale) * v) * gScale;
}
//*********************************************************************************************************



//*********************************************************************************************************
//Evaluate the derivative of the geodesic leaving p in the direction of v
//*********************************************************************************************************
float4 evalTangentRay(float4 p, float4 v, float t)
{
    p = normalize(p);
    v = normalize(v);
    return (-sin(t / gScale) * p + cos(t / gScale) * v);
}
//*********************************************************************************************************



//*********************************************************************************************************
//Evaluate the derivative of the geodesic leaving p in the direction of v
//*********************************************************************************************************
sphericalRay evalRay(sphericalRay sRay, float t)
{
    sphericalRay newRay = sRay;
    
    newRay.origin = evalRay(sRay.origin, sRay.dir, t);
    newRay.dir = evalTangentRay(sRay.origin, sRay.dir, t);

    return newRay;
}
//*********************************************************************************************************



//*********************************************************************************************************
//paralell transport of v0 from Tp0S (tangent space in p0) to Tp1S (tangent space in p1)
//*********************************************************************************************************
float4 sphericalTransport(float4 p0, float4 p1, float4 v0)
{
    v0 = normalize(v0);

    //u0 is the tangent at p0 of the geodesic from p0 to p1
    float4 u0 = p1 - p0;
    u0 = normalize(project(u0, p0));
    
    //w0 is orthogonal to u0, producing a 2-basis at p0 for the vector v0
    float4 w0 = normalize(project(v0, u0));

    //this angle should be mantained along the geodesic
    float transportAngle = acos(dot(v0, u0));

    //length of the geodesic
    float lenght = acos(dot(p1, p0) / (gScale * gScale)) * gScale;

    //tangent at p1 of the geodesic
    float4 u1 = evalTangentRay(p0, u0, lenght); //-sin(lenght) * p0 + cos(lenght) * u0;
    u1 = normalize(u1);

    // the transport of w0 from Tp0S to Tp1S is constant, since it is orthorgonal to the geodesic
    float4 w1 = w0;

    // transport of u0 from Tp0S to Tp1S through the geodesic
    return cos(transportAngle) * u1 + sin(transportAngle) * w1;
}
//*********************************************************************************************************



//*********************************************************************************************************
//
//*********************************************************************************************************
float4 flatToSpherical(float3 p)
{
    return normalize(float4(p * gSphericalScale, 1.f))*gScale;
}
//*********************************************************************************************************



//*********************************************************************************************************
//
//*********************************************************************************************************
float3 sphericalToFlat(float4 P)
{
    return (P.xyz / P.w) / gSphericalScale;
}
//*********************************************************************************************************



//*********************************************************************************************************
//maps tangent vectors of the tangent space in p (TpE) to tangent vectors in p/|p| 
//at Tp/|p|S (tangent space of the 3-sphere)
//*********************************************************************************************************
float4 flatToSpherical(float3 p, float3 v)
{
    float4 sp = float4(p * gSphericalScale, 1.f);
    float4 sv = float4(normalize(v), 0.f);
    /*
    float4 Dx = float4(dot(P, P) - P.x * P.x, -P.y * P.x, -P.z * P.x, -P.x);
    float4 Dy = float4(-P.y * P.x, dot(P, P) - P.y * P.y, -P.z * P.y, -P.y);
    float4 Dz = float4(-P.z * P.x, -P.z * P.y, dot(P, P) - P.z * P.z, -P.z);
    
    return normalize(V.x * Dx + V.y * Dy + V.z * Dz);
   
    The above calculation is referent to the differential map of the parameterization,
    which is equivalent to project V on the tangent space (calculation below).
    */
    return normalize(project(sv, sp));
}
//*********************************************************************************************************



//*********************************************************************************************************
//maps tangent vectors of the tangent space in p (TpE) to tangent vectors in p/|p| 
//at Tp/|p|S (tangent space of the 3-sphere)
//*********************************************************************************************************
float3 sphericalToFlat(float4 sp, float4 sv)
{
    sv = normalize(sv);

    //Dx, Dy, Dz, and Dw are the directional derivatives multiplied by P.w
    float3 Dx =  float3(1.f, 0.f, 0.f);
    float3 Dy =  float3(0.f, 1.f, 0.f);
    float3 Dz =  float3(0.f, 0.f, 1.f);
    float3 Dw = - sp.xyz / sp.w;
    
    return normalize(sv.x * Dx + sv.y * Dy + sv.z * Dz + sv.w * Dw );
}
//*********************************************************************************************************



//*********************************************************************************************************
//computes the first intersection point between the geodesic cost*p+sint*v
//and the 2-sphere orthogonal to N
//*********************************************************************************************************
sphericalRay raySphereIntersection
(sphericalRay sRay, float4 sn /*4-vector in the 3-sphere othorgonal to a 2-sphere*/)
{
    //parameter of the geodesic cos*origin+sin*dir which intersects the 2-sphere
    float hitAngle = atan(-dot(normalize(sRay.origin), sn) / dot(sRay.dir, sn)) * gScale;

    sphericalRay newRay = sRay;

    newRay = evalRay(sRay, hitAngle);

    // The segment is parallel to plane or point is close or behind to the plane
    if (dot(sRay.dir, sn) * dot(sRay.dir, sn) < gEpsilon || hitAngle < gEpsilon)
    {
        newRay.check = 0; //not valid
    }
    else
    {
        newRay.check = 1;
    }
    //newRay.origin = cos(hitAngle) * sRay.origin + sin(hitAngle) * sRay.dir;
    //newRay.dir = -sin(hitAngle) * sRay.origin + cos(hitAngle) * sRay.dir;

    return newRay;
}
//*********************************************************************************************************
//*********************************************************************************************************



//*********************************************************************************************************
//computes the first intersection point between the geodesic cost*p+sint*v
//and the 2-sphere orthogonal to N
//*********************************************************************************************************
/*sphericalRay rayGeodesicIntersection(sphericalRay sRay, float4 n1, float4 n2, float r)
{
    sphericalRay newRay = sRay;
    
    float4 p = sRay.origin;
    float4 v = sRay.dir;

    float hitAngle = acos(r * (dot(n2, v) - dot(n1, v)) / (dot(n1, p) * dot(n2, v) - dot(n1, v) * dot(n2, p)));

    float d = (dot(n1, p) * dot(n2, v) - dot(n1, v) * dot(n2, p));
    if (d*d<gEpsilon || hitAngle>gEpsilon)
    {
        newRay.border = 0;
    }
    else
    {
        newRay.border = 1;
    }
    newRay.origin = cos(hitAngle) * sRay.origin + sin(hitAngle) * sRay.dir;
    newRay.dir = -sin(hitAngle) * sRay.origin + cos(hitAngle) * sRay.dir;

    return newRay;
}*/
//*********************************************************************************************************



//*********************************************************************************************************
//maps the euclidean ray to the spherical ray
//********************************************************************************************************* 
sphericalRay flatToSpherical(euclideanRay ray)
{
    sphericalRay sRay;

    sRay.origin = flatToSpherical(ray.origin);
    sRay.dir    = flatToSpherical(ray.origin, ray.dir);
    
    sRay.check  = ray.check;
    sRay.hit    = ray.hit;
    sRay.border = ray.border;
    sRay.dist   = ray.dist;

    return sRay;
}
//*********************************************************************************************************



//*********************************************************************************************************
//
//*********************************************************************************************************
euclideanRay sphericalToFlat(sphericalRay sRay)
{
    euclideanRay ray;

    ray.origin = sphericalToFlat(sRay.origin);
    ray.dir    = sphericalToFlat(sRay.origin, sRay.dir);

    ray.check  = sRay.check;
    ray.hit    = sRay.hit;
    ray.border = sRay.border;
    ray.dist   = sRay.dist;

    return ray;
}
//*********************************************************************************************************



//*********************************************************************************************************
//maps the ray to the correspondent in the opposite dodecahedral face with a adition pi/5 rotation,
//this transformation gives rise to the Poincare dodecaheral space
//*********************************************************************************************************
euclideanRay PoincareSphereMap(euclideanRay ray, float3 n)
{
    float3 p = ray.origin;
    float3 v = ray.dir;

    //face barycenter
    n = normalize(n);
    float3 q = n;

    float t = dot(n, p);
    
    //push p to the dodecahedron face, case it is not
    p = p / t;

    //translating  p to the plane crossing the origin
    float3 proj_p = p - q;

    float x1 = length(proj_p);

    //face basis
    float3 e1 = normalize(proj_p);
    float3 e2 = cross(n, e1);

    //rotating 2PI/10
    float theta = 3.14159265359 / 5.f;
    p = (cos(theta) * e1 + sin(theta) * e2) * x1;

    p -= q;

    euclideanRay newRay = ray;
    newRay.origin = p;
    
    return newRay;
}
//*********************************************************************************************************



//*********************************************************************************************************
//
//*********************************************************************************************************
euclideanRay PoincareSphereMap(euclideanRay ray, float4 sn)
{
    return PoincareSphereMap(ray, normalize(sn.xyz));
}
//*********************************************************************************************************



//*********************************************************************************************************
//
//*********************************************************************************************************
float4 PoincareUpdateDir(sphericalRay prevRay, sphericalRay nexRay, float4 sN)
{
    float4 sp0 = prevRay.origin;
    float4 sv0 = prevRay.dir;
    
    float4 sp1 = nexRay.origin;
    float4 sv1;
    
    float3 b0 = normalize(sN.xyz);
    float4 sb0 = flatToSpherical(b0);
    float4 sb1 = flatToSpherical(-b0);

    float4 transp_sv0 = sphericalTransport(sp0, sb0, sv0);

    float3 transp_v0 = sphericalToFlat(sb0, transp_sv0);
  
    //update v by a rotation of pi/5 around n
    float xn = dot(transp_v0, b0);
    float3 pv = project(transp_v0, b0);
    float xpv = length(pv);
    float3 e1 = normalize(pv);
    float3 e2 = cross(b0, e1);
    
     //rotating 2PI/10
    float theta = 3.14159265359f / 5.f;
    pv = (cos(theta) * e1 + sin(theta) * e2) * xpv;

    float3 v1 = normalize(pv + xn * b0);

    sv1 = flatToSpherical(b0, v1);
    
    sv1 = sphericalTransport(sb0, sb1, sv1);
    sv1 = sphericalTransport(sb1, sp1, sv1);

    return sv1;
}
//*********************************************************************************************************


//*********************************************************************************************************
//
//*********************************************************************************************************
sphericalRay PoincareSphere(sphericalRay sRay, float4 sn)
{
    euclideanRay ray = sphericalToFlat(sRay);
    
    float3 n = normalize(sn.xyz);

    euclideanRay new_fRay = PoincareSphereMap(ray, n);

    sphericalRay new_sRay = sRay;
    new_sRay.origin = flatToSpherical(new_fRay.origin);
    new_sRay.dir = PoincareUpdateDir(sRay, new_sRay, sn); //flatToSpherical(new_fRay.origin, new_fRay.dir); //sphericalTransport(sRay.origin, new_sRay.origin, sRay.dir);
    
    return new_sRay;
}
//*********************************************************************************************************



//*********************************************************************************************************
//
//*********************************************************************************************************
sphericalRay sphericalReflection(sphericalRay sRay, float4 N)
{
    float3 n = normalize(N.xyz);

    euclideanRay ray = sphericalToFlat(sRay);
    
    sphericalRay newRay = sRay;

    float4 sn = flatToSpherical(ray.origin, n);
    float4 sv = sRay.dir;
    
    newRay.dir = normalize(reflect(sv, sn));
    
    return newRay;
}
//*********************************************************************************************************



//*********************************************************************************************************
//
//*********************************************************************************************************
sphericalRay traceSphericalRay( in sphericalRay sRay )
{
    sphericalRay new_sRay = sRay;
   
    //used to verify if the ray intersects an edge
    float tdist = 1000000.0;
    float tdist2 = 2000000.0;
    float tdist3 = 3000000.0;
    
    //index of the closest face which intersects sRay
    int hit_id = -1;
    
    //list of the distances between sRay and the 12 dodecahedral faces
    float dist[12];
    
    for (int i = 0; i < 12; ++i)
    {
        dist[i] = 1000000.0;

        //get the spherical normal of the face with index i
        float4 sn = flatToSpherical(normalize(dodecahedronNormals[i].xyz), normalize(dodecahedronNormals[i].xyz));

        sphericalRay next_sRay = raySphereIntersection(sRay, sn);
       
        //if the intersection is valid
        if (next_sRay.check == 1)
        {
           // dist[i] = sDistance(flatToSpherical(next_sRay.origin), sRay.origin);
           dist[i] = sDistance(next_sRay.origin, sRay.origin);
            if (tdist > dist[i])
            {
                tdist3 = tdist2;
                tdist2 = tdist;
                tdist = dist[i];
                hit_id = i;
                
                new_sRay.origin = next_sRay.origin;
                new_sRay.dir    = next_sRay.dir;
                new_sRay.check  = next_sRay.check;

                new_sRay.hit = 1;
            }
            else if (tdist2 > dist[i])
            {
                tdist3 = tdist2;
                tdist2 = dist[i];
            }
        }
    }

    new_sRay.dist = tdist;
    
    //the Poincare transformation
    new_sRay = PoincareSphere(new_sRay, dodecahedronNormals[hit_id]);
    //new_sRay = sphericalReflection(new_sRay, dodecahedronNormals[hit_id]);
    
    //is it on the border?
    if (tdist2 - tdist < gThickness)
    {
        new_sRay.border = 1;
    }
    else
    {
        new_sRay.border = 0;
    }

    return new_sRay;
}
//*********************************************************************************************************



//*********************************************************************************************************
//
//*********************************************************************************************************
bool updateEuclideanRayInSphericalSpace(inout float3 p, inout float3 v)
{
    euclideanRay ray;
    ray.origin = p;
    ray.dir = v;
    
    sphericalRay sRay = flatToSpherical(ray);
    sRay.check = 0.f;
    sRay.border = 0.f;
    sRay.hit = 0.f;

    ray = sphericalToFlat(traceSphericalRay(sRay));
    p = ray.origin;
    v = ray.dir;

    return bool(ray.border);
}
//*********************************************************************************************************



//*********************************************************************************************************
//
//*********************************************************************************************************
float3 traceSphericalShade(float3 p, float3 v)
{
    float g_distance = 0;  
                
    euclideanRay ray;
    ray.origin = p;
    ray.dir = v;
    
    sphericalRay sRay =  flatToSpherical(ray);                    
    sRay.check = 0.f;
    sRay.border = 0.f;
    sRay.hit = 0.f;

    for (int ci = 0; ci < gMaxDepth; ++ci)
    {
        /*
        sphericalRay hitGeo = rayGeodesicIntersection(sRay, N1, N2, 0.001);
        if (hitGeo.border = 1)
        {
            return float3(0.f, 0.f, 0.f);
        }
        */
        sRay.dist = 0.f;
        
        sRay = traceSphericalRay(sRay);
       
        if (sRay.border == 1 || sRay.hit == 0)
        {
           g_distance = g_distance + sRay.dist;
           break;
        }
        
        g_distance = g_distance + sRay.dist;
    }
    
    //float f = exp(1.e-4f * g_distance * g_distance);

    if (sRay.border == 1)
    {
        float f = (1.f - g_distance / (6.28318*gScale));
        return (float3(182.f, 82.f, 45.f) / 255.f) * (1.f - g_distance / (6.28318 * gScale)) * f * f * f;
    }

    if (sRay.check == 1)
    {
        //return float3(0.f, 1.f, 0.f); //modificacao do ospecuolar
        return float3(0.f, 0.f, 0.f); //modificacao do ospecuolar
    }
    else
    {
        return float3(0.0, 0.0, 0.0);
    }
}
//*********************************************************************************************************



//*********************************************************************************************************
//*********************************************************************************************************
//Clifford Torus
//*********************************************************************************************************
//
//*********************************************************************************************************
/*sphericalRay shadeCliffordTorus(sphericalRay sRay)
{
    float4 p = sRay.origin;
    float4 v = sRay.dir;

    float a = p.x * p.x + p.y * p.y - (v.x * v.x + v.y * v.y);
    float c = p.z * p.z + p.w * p.w - (v.z * v.z + v.w * v.w);
    float b = p.x * v.x + p.y * v.y;
    float d = p.z * v.z + p.w * v.w;
    
    float e = 0.5 - (v.x * v.x + v.y * v.y);
    float f = 0.5 - (v.z * v.z + v.w * v.w);

    sphericalRay newRay = sRay;

    if (abs(a * d - b * c) < 0.00001)
    {
        newRay.check = 0;
    }
    else
    {
        newRay.check = 1;
    }

    float res = (1.f / (a * d - b * c)) * (-c * e + a * f);

    if (res > 0.01)
    {
        newRay.check = 1;
    }
    else
    {
        newRay.check = 0;
    }

    float t = acos(res) / 2.f;
    newRay = evalRay(newRay, t);

    return newRay;
}


float3 shadeCliffordTorus(float3 p, float3 v)
{
    float g_distance = 0;
                
    euclideanRay ray;
    ray.origin = p;
    ray.dir = v;
    
    sphericalRay sRay = flatToSpherical(ray);
    sRay.check = 0.f;
    sRay.border = 0.f;
    sRay.hit = 0.f;

    sphericalRay newRay = shadeCliffordTorus(sRay);
    
    if (newRay.check == 1)
    {
        //return float3(0.f, 1.f, 0.f); //modificacao do ospecuolar
        return float3(0.f, 0.f, 0.f); //modificacao do ospecuolar
    }
    else
    {
        g_distance = sDistance(sRay.origin, newRay.origin);
        float f = (1.f - g_distance / (6.28318 * gScale));
        return (float3(182.f, 82.f, 45.f) / 255.f) * (1.f - g_distance / (6.28318 * gScale)) * f * f * f;
    }
}*/





//*********************************************************************************************************
//*********************************************************************************************************



//*********************************************************************************************************
//the functions below are used for the flat case of the poincare sphere
//*********************************************************************************************************
euclideanRay func_plane_ray_intersection
(
  in float3 po, //plane origin
  in float3 pn, //plane dir
    
  in euclideanRay ray
/*
  in vec3 rayo, //ray origin
  in vec3 rayn, //ray direction
  out vec3 hitpos,
  out int check*/
)
{
    euclideanRay hitRay = ray;

    hitRay.origin = float3(0.0, 0.0, 0.0);
    
    float3 w = ray.origin - po;
    float D = pn.x * ray.dir.x + pn.y * ray.dir.y + pn.z * ray.dir.z;
    float N = -(pn.x * w.x + pn.y * w.y + pn.z * w.z);
    hitRay.check = 0;

    if (D * D <= 0.00001)   // The segment is parallel to plane
    {
        if (N == 0.0)
        {
            hitRay.check = 2; // The segment lies in plane
        }
        else
        {
            hitRay.check = 0; //no intersection
        }
    }
        
    if (N < 0.0)
    {
        hitRay.check = 0;
    }
        
    //compute the intersection parameter
    float t = N / D;
    hitRay.origin = ray.origin + t * ray.dir;

    if (t <= 0.001)
        hitRay.check = 3; //The intersection point  lies outside the segment, so there is no intersection
    else
        hitRay.check = 1; //normal intersection

    return hitRay;
}
//*********************************************************************************************************



//*********************************************************************************************************
euclideanRay traceFlatRay
(
in euclideanRay ray
/*inout vec3 rayo,
inout vec3 rayn,
out int border,
out int hit,
out float outdist*/
)
{
    euclideanRay new_Ray = ray;

    //used to verify if the ray intersects an edge
    float tdist = 1000000.0;
    float tdist2 = 2000000.0;
    float tdist3 = 3000000.0;
    
    //index of the closest face which intersects sRay
    int hit_id = -1;
    
    //list of the distances between sRay and the 12 dodecahedral faces
    float dist[12];
    
    for (int i = 0; i < 12; ++i)
    {
        dist[i] = 1000000.0;
      
        euclideanRay next_Ray = func_plane_ray_intersection(normalize(dodecahedronNormals[i].xyz) * 0.5, normalize(dodecahedronNormals[i].xyz), ray);
        
        //if the intersection is valid
        if (next_Ray.check == 1)
        {
            dist[i] = length(next_Ray.origin - ray.origin);
            if (tdist >= dist[i])
            {
                tdist3 = tdist2;
                tdist2 = tdist;
                tdist = dist[i];
                hit_id = i;

                new_Ray.origin = next_Ray.origin;
                new_Ray.check = next_Ray.check;

                new_Ray.hit = 1;
            }
        }
    }

    new_Ray.dist = tdist;
    
    //the Poincare transformation
    new_Ray = PoincareSphereMap(new_Ray, dodecahedronNormals[hit_id]);
   
    //is it on the border?
    if (tdist2 - tdist < 0.12)
    {
        new_Ray.border = 1;
    }
    else
    {
        new_Ray.border = 0;
    }

    return new_Ray;
}
//*********************************************************************************************************



//*********************************************************************************************************
float3 traceFlatShade(float3 p, float3 v)
{
    float uf_fog_parameter = 0.1;
    float g_distance = 0;
                    
    float o_specular = 0.0;
                
    euclideanRay ray;
    ray.origin = p;
    ray.dir = v;
    ray.check = 0;
    ray.border = 0;
    ray.hit = 0;
                    
    for (int ci = 0; ci < gMaxDepth; ++ci)
    {
        ray.dist = 0.f;
        ray = traceFlatRay(ray);
        if (ray.border == 1 || ray.hit == 0)
        {
            g_distance = g_distance + ray.dist;
            break;
        }
        
        g_distance = g_distance + ray.dist;
    }
    float tmp = exp(-g_distance / uf_fog_parameter);
    
    if (ray.border == 1)
    {
        return float3(1.f, 0.f, 0.f);
    }

    if (ray.check == 1)
    {
        return float3(tmp * o_specular * o_specular * o_specular * o_specular, tmp, 0.0); //modificacao do ospecuolar
    }
    else
    {
        return float3(0.0, 0.0, 0.0);
    }
}
//*********************************************************************************************************
//*********************************************************************************************************