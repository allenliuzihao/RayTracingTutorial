#pragma once

#include <iostream>

#include "vec3.h"
#include "ray.h"
#include "material.h"

class dielectric : public material {
public:
	dielectric(double ri) : ref_idx(ri) {}

    virtual bool scatter(const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered) const override {
        attenuation = color(1.0, 1.0, 1.0);
        double etai_over_etat = rec.front_face ? (1.0 / ref_idx) : ref_idx;
        vec3 unit_direction = unit_vector(r_in.direction());

        double cos_theta = fmax(0.0, fmin(1.0,  dot(-unit_direction, rec.normal)));
        double sin_theta = std::sqrt(1.0 - cos_theta * cos_theta);

        if (etai_over_etat * sin_theta > 1.0) {
            vec3 reflected = reflect(unit_direction, rec.normal);
            scattered = ray(rec.p, reflected, r_in.time());
        } else {
            double reflect_prob = schlick(cos_theta, etai_over_etat);
            if (random_double() < reflect_prob) {
                vec3 reflected = reflect(unit_direction, rec.normal);
                scattered = ray(rec.p, reflected, r_in.time());
                return true;
            }

            vec3 refracted = refract(unit_direction, rec.normal, etai_over_etat);
            scattered = ray(rec.p, refracted, r_in.time());
        }

        return true;
    }

private:
	double ref_idx;
};