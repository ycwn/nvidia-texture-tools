
namespace nv {

    class Color32;
    struct ColorBlock;
    struct BlockDXT1;
    class Vector3;

    // All these functions return MSE.

    float compress_dxt1_single_color_optimal(Color32 c, BlockDXT1 * output);
    float compress_dxt1_single_color_optimal(const Vector3 & color, BlockDXT1 * output);

    float compress_dxt1_single_color(const Vector3 * colors, const float * weights, int count, const Vector3 & color_weights, BlockDXT1 * output);
    float compress_dxt1_least_squares_fit(const Vector3 input_colors[16], const Vector3 * colors, const float * weights, int count, const Vector3 & color_weights, BlockDXT1 * output);
    float compress_dxt1_bounding_box_exhaustive(const Vector3 input_colors[16], const Vector3 * colors, const float * weights, int count, const Vector3 & color_weights, int search_limit, BlockDXT1 * output);
    void compress_dxt1_cluster_fit(const Vector3 input_colors[16], const Vector3 * colors, const float * weights, int count, const Vector3 & color_weights, BlockDXT1 * output);


    float compress_dxt1(const Vector3 colors[16], const float weights[16], const Vector3 & color_weights, BlockDXT1 * output);

}
