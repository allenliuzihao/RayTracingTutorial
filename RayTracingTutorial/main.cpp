#include <windows.h>

#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>

#include "utilities.h"

#include "color.h"
#include "hittables.h"
#include "sphere.h"
#include "camera.h"

#include "material.h"
#include "lambertian.h"
#include "metal.h"
#include "dialectric.h"

color ray_color(const ray& r, const hittable& world, int depth) {
    hit_record rec;

    if (depth <= 0) {
        return color(0, 0, 0);
    }

    if (world.hit(r, 0.001, infinity, rec)) {
        ray scatter;
        color attenuation;
        if (rec.mat_ptr->scatter(r, rec, attenuation, scatter)) {
            return attenuation * ray_color(scatter, world, depth - 1);
        }
        return color(0, 0, 0);
    }

    vec3 unit_dir = unit_vector(r.direction());
    double t = 0.5 * (unit_dir.y() + 1.0);
    return (1.0 - t) * color(1.0, 1.0, 1.0) + t * color(0.5, 0.7, 1.0);
}

hittables random_scene() {
    hittables world;

    auto ground_material = std::make_shared<lambertian>(color(0.5, 0.5, 0.5));
    world.add(std::make_shared<sphere>(point3(0, -1000, 0), 1000, ground_material));

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            auto choose_mat = random_double();
            point3 center(a + 0.9 * random_double(), 0.2, b + 0.9 * random_double());

            if ((center - point3(4, 0.2, 0)).length() > 0.9) {
                std::shared_ptr<material> sphere_material;

                if (choose_mat < 0.8) {
                    // diffuse
                    auto albedo = color::random() * color::random();
                    sphere_material = std::make_shared<lambertian>(albedo);
                    world.add(std::make_shared<sphere>(center, 0.2, sphere_material));
                }
                else if (choose_mat < 0.95) {
                    // metal
                    auto albedo = color::random(0.5, 1);
                    auto fuzz = random_double(0, 0.5);
                    sphere_material = std::make_shared<metal>(albedo, fuzz);
                    world.add(std::make_shared<sphere>(center, 0.2, sphere_material));
                }
                else {
                    // glass
                    sphere_material = std::make_shared<dielectric>(1.5);
                    world.add(std::make_shared<sphere>(center, 0.2, sphere_material));
                }
            }
        }
    }

    auto material1 = std::make_shared<dielectric>(1.5);
    world.add(std::make_shared<sphere>(point3(0, 1, 0), 1.0, material1));

    auto material2 = std::make_shared<lambertian>(color(0.4, 0.2, 0.1));
    world.add(std::make_shared<sphere>(point3(-4, 1, 0), 1.0, material2));

    auto material3 = std::make_shared<metal>(color(0.7, 0.6, 0.5), 0.0);
    world.add(std::make_shared<sphere>(point3(4, 1, 0), 1.0, material3));

    return world;
}

void render(std::mutex& m,
            int core,
            int samples_per_pixel, int max_depth,
            vec3** image_grid,
            const hittables& world,
            const camera& cam,
            const std::pair<int, int> tile_origin, 
            const std::pair<int, int> tile_dim, 
            const std::pair<int, int> image_dim) {
    auto thread_id = std::this_thread::get_id();
    auto mask = (static_cast<DWORD_PTR>(1) << core);            
    if (SetThreadAffinityMask(GetCurrentThread(), mask) == 0) {
#ifdef PRINT_LOG
        {
            std::lock_guard<std::mutex> lock(m);
            std::cerr << "Thread #" << thread_id << ": error setting affinity mask for thread and core: " << core << "\n";
        }
#endif
        return;
    }

    int row_bound = min(image_dim.first, tile_origin.first + tile_dim.first);
    int col_bound = min(image_dim.second, tile_origin.second + tile_dim.second);

#ifdef PRINT_LOG
    {
        std::lock_guard<std::mutex> lock(m);
        std::cerr << "Thread #" << thread_id
                  << ": work on tile " << core
                  << " with origin row " << tile_origin.first << " col " << tile_origin.second << "\n";
    }
#endif

    for (int row = tile_origin.first; row < row_bound; ++row) {
        for (int col = tile_origin.second; col < col_bound; ++col) {            
            for (int sample = 0; sample < samples_per_pixel; ++sample) {
                double u = (col * 1.0 + random_double()) / image_dim.second;
                double v = (row * 1.0 + random_double()) / image_dim.first;
                ray r = cam.get_ray(u, v);
                image_grid[row][col] += ray_color(r, world, max_depth);
            }
        }
    }

#ifdef PRINT_LOG
    {
        std::lock_guard<std::mutex> lock(m);
        std::cerr << "\nThread #" << thread_id 
                  << " tile " << core << " finished\n";
    }
#endif
}

int main() {
    // renderer configuration
    auto aspect_ratio = 3.0 / 2.0;
    auto image_width = 200; //1200
    auto image_height = static_cast<int>(image_width / aspect_ratio);
    auto samples_per_pixel = 10;   // 500
    auto max_depth = 50;

    // Scene and materials
    auto world = random_scene();
    
    auto material_ground = std::make_shared<lambertian>(color(0.8, 0.8, 0.0));
    auto material_center = std::make_shared<lambertian>(color(0.1, 0.2, 0.5));
    auto material_left = std::make_shared<dielectric>(1.5);
    auto material_right = std::make_shared<metal>(color(0.8, 0.6, 0.2), 0.0);

    world.add(std::make_shared<sphere>(point3(0.0, -100.5, -1.0), 100.0, material_ground));
    world.add(std::make_shared<sphere>(point3(0.0, 0.0, -1.0), 0.5, material_center));
    world.add(std::make_shared<sphere>(point3(-1.0, 0.0, -1.0), 0.5, material_left));
    world.add(std::make_shared<sphere>(point3(-1.0, 0.0, -1.0), -0.45, material_left));
    world.add(std::make_shared<sphere>(point3(1.0, 0.0, -1.0), 0.5, material_right));

    // Camera
    point3 lookfrom(13, 2, 3);
    point3 lookat(0, 0, 0);
    vec3 vup(0, 1, 0);
    auto dist_to_focus = 10.0;
    double aperture = 0.1;

    camera cam(lookfrom, lookat, vup, 20.0, aspect_ratio, aperture, dist_to_focus);

    // prepare image grid
    vec3** image_grid = new vec3 * [image_height];
    for (int row = 0; row < image_height; ++row) {
        image_grid[row] = new vec3[image_width];
        for (int col = 0; col < image_width; ++col) {
            image_grid[row][col] = color(0, 0, 0);
        }
    }

    // tile width and height
    unsigned num_cpus_context = std::thread::hardware_concurrency();
    if (num_cpus_context == 0) {
        throw std::runtime_error("unable to determine the number of cores on the CPU.");
    }

    std::vector<int> factors = find_closest_factors(num_cpus_context);
    int num_tiles_horizontal = factors[0], num_tiles_vertical = factors[1];

    if (image_width < image_height) {
        num_tiles_horizontal = factors[1];
        num_tiles_vertical = factors[0];
    }

    int tile_width = (int)ceil(image_width / (double) num_tiles_horizontal);
    int tile_height = (int)ceil(image_height / (double) num_tiles_vertical);
    
    int t_index = 0;
    std::mutex m;
    std::vector<std::thread> threads(num_cpus_context);

    // TODO: parallelize this on cpu cores, with one thread per core.
    std::cerr << "start to render the image in tiles of dim height: " << tile_height << " width: " << tile_width << " image height: " << image_height << " image width: " << image_width << "\n";
    std::cerr << "Launching " << num_cpus_context << " render threads.\n";
    
    std::clock_t c_start = std::clock();
    auto t_start = std::chrono::high_resolution_clock::now();

    for (int row = 0; row < image_height; row += tile_height) {
        for (int col = 0; col < image_width; col += tile_width) {
            threads[t_index] = std::thread(render,
                                           std::ref(m),
                                           t_index,                                           
                                           samples_per_pixel, max_depth, 
                                           image_grid,
                                           world, cam, 
                                           std::make_pair(row, col), 
                                           std::make_pair(tile_height, tile_width), 
                                           std::make_pair(image_height, image_width));            
            t_index++;
        }
    }

    for (auto& t : threads) {
        t.join();
    }
    
    std::clock_t c_end = std::clock();
    auto t_end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> diff = t_end - t_start;

    std::cerr << "\nrendering complete.\n CPU time used: "
              << 1000.0 * ((double) c_end - (double) c_start) / CLOCKS_PER_SEC << " ms\n"
              << "Wall clock time passed: "
              << std::chrono::duration<double, std::milli>(t_end - t_start).count()
              << " ms\n";

    std::cerr << "\nsaving ppm images\n";
    std::cout << "P3\n" << image_width << ' ' << image_height << "\n255\n";
    for (int row = image_height - 1; row >= 0; --row) {
        for (int col = 0; col < image_width; ++col) {
            write_color(std::cout, image_grid[row][col], samples_per_pixel);
        }
    }
    std::cerr << "\nDone.\n";

    for (int row = 0; row < image_height; ++row) {
        delete[] image_grid[row];
    }
    delete[] image_grid;
}