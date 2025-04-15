#include <iostream>
#include <fstream>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <glm/mat4x4.hpp>

namespace po = boost::program_options;

inline std::vector<std::string> split_string(const std::string& str, const std::string& delimiter)
{
    std::vector<std::string> strings;

    std::string::size_type pos = 0;
    std::string::size_type prev = 0;
    while ((pos = str.find(delimiter, prev)) != std::string::npos)
    {
        strings.push_back(str.substr(prev, pos - prev));
        prev = pos + delimiter.size();
    }

    strings.push_back(str.substr(prev));

    return strings;
}

std::tuple<std::string, std::string> gen_mitsuba_header(std::string const& version)
{
    std::string begin = "<scene version=\"" + version + "\">\n";
    std::string end = "</scene>\n";
    return { begin, end };
}

void gen_camera_params(float const fov, float const aperture_radius)
{
    std::ofstream file("camera_params.xml");
    auto [scene_begin, scene_end] = gen_mitsuba_header("3.0.0");
    file << scene_begin;
    //file << "<string name=\"fov_axis\" value=\"smaller\" />\n";
    file << R"(<float name="aperture_radius" value=")" << aperture_radius << "\" />\n";
    file << R"(<float name="fov" value=")" << fov << "\" />\n";
    file << scene_end;
    file.close();
}

void gen_camera_batch(int const rows, int const cols, float const offset, float const focus_distance)
{
    int total_views = rows * cols;

    std::ofstream file("scene_batch.xml");
    file << "<sensor type=\"batch\">\n";
    file << "\t<integer name=\"rows\" value=\"1\" />\n";
    file << "\t<integer name=\"cols\" value=\"" << cols << "\" />\n";
    file << "\t<integer name=\"total_views\" value=\"" << total_views << "\" />\n";
    file << "\t<float name=\"offset\" value=\"" << offset << "\" />\n";
    file << "\t<float name=\"focus_distance\" value=\"" << focus_distance << "\" />\n";
    file << "\t<include filename=\"small_$row.xml\" />\n";
    file << R"(
    <sampler type="independent">
        <integer name="sample_count" value="$spp"/>
    </sampler>
    )";

    file << R"(
    <film type="hdrfilm" id="film">
        <integer name="width" value="$width"/>
        <integer name="height" value="$height"/>
        <string name="file_format" value="openexr" />
		<string name="pixel_format" value="rgb" />
        <rfilter type="rdepth">
            <float name="stddev" value="$rd" />
        </rfilter>
    </film>)";

    file << "\n</sensor>\n";

    file.close();
}

int main(int argc, char** argv)
{
    int rows = 0;
    int cols = 0;
    float offset = 0.0f;
    float aperture_radius = 0.0f;
    float fov = 0.0f;
    float focus_distance = 0.0f;
    std::string to_world_str = "";
    std::string name = "";

    po::options_description desc("Allowed options");
    desc.add_options()
        ("rows,r", po::value<int>(&rows), "Number of rows")
        ("cols,c", po::value<int>(&cols), "Number of columns")
        ("offset,o", po::value<float>(&offset), "Distance between cameras")
        ("to_world", po::value<std::string>(&to_world_str), "To world transform")
        ("name,n", po::value<std::string>(&name), "Name of the output file")
        ("radius,a", po::value<float>(&aperture_radius), "Aperture radius")
        ("fov,f", po::value<float>(&fov), "Field of view")
        ("distance,d", po::value<float>(&focus_distance), "Focus distance")
        ("help,h", "Produce help message");

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
    po::notify(vm);

    if (vm.contains("help"))
    {
        std::cout << desc << "\n";
        return 1;
    }

    glm::mat4 to_world = glm::mat4(1.0f);
    std::vector<std::string> const to_world_parts = split_string(to_world_str, " ");
    if (to_world_parts.size() != 16)
    {
        std::cerr << "Invalid to_world transform\n";
        return 1;
    }

    for (int i = 0; i < 16; i++)
        to_world[i / 4][i % 4] = std::stof(to_world_parts[i]);

    to_world = glm::transpose(to_world);
    glm::vec3 const origin = glm::vec3(to_world[3]);
    glm::vec3 const right = glm::vec3(to_world[0]);

    gen_camera_batch(rows, cols, offset, focus_distance);
    gen_camera_params(fov, aperture_radius);

    int cameras = rows * cols;
    int mid_point = cameras / 2;

    int local_id = 0;
    for (int i = -mid_point, id = 1; i < mid_point; i += cols, ++id)
    {
        std::ofstream file(name + "_" + std::to_string(id) + ".xml");
        auto [scene_begin, scene_end] = gen_mitsuba_header("3.0.0");
        file << scene_begin;
        for (int j = 0; j < cols; j++)
        {
            float t = 1.0f - (float)local_id++ / (float)(cameras - 1);
            file << "<sensor type=\"thinlens\">\n";
            file << "\t<float name=\"t\" value=\"" << std::to_string(t) << "\" />\n";
            file << "\t<include filename=\"camera_params.xml\" />\n";
            file << "\t<transform name=\"to_world\">\n";

            glm::vec3 new_point = origin - (i + j) * offset * right;

            file << "\t\t<matrix value=\"";
            auto new_to_world = to_world;
            new_to_world[3] = glm::vec4(new_point, 1.0f);
            new_to_world = glm::transpose(new_to_world);
            for (int k = 0; k < 16; k++)
            {
                file << new_to_world[k / 4][k % 4];
                if (k != 15)
                    file << " ";
            }
            file << "\"/>\n";

            file << "\t</transform>\n";
            file << "\t<include filename=\"camera_film.xml\" />\n";
            file << "</sensor>\n\n";
        }
        file << scene_end;
        file.close();
    }

    return 0;
}
