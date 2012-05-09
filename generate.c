#define _GNU_SOURCE

#include<stdlib.h>
#include<stdint.h>
#include<stdio.h>
#include<math.h>

#include<png.h>

#include<gsl/gsl_sys.h>
#include<gsl/gsl_math.h>
#include<gsl/gsl_integration.h>

unsigned int HEIGHT = 1000;
unsigned int WIDTH = 1000;

double LSF_WIDTH = 3.0;
double LSF_HEIGHT = 3.0;

typedef struct {
        double slope;
        double x;
        double y;
        double x2;
} our_integration_params;

gsl_integration_workspace* w1;
gsl_integration_workspace* w2;

static double lsf(double x) {
        return exp(-(x*x) * 100.0) / 28.0 +
                exp(-(x*x) * 5.0) / 200.0;
}

static double src(double x, double y, double slope) {
        if (y - slope * (x - WIDTH / 2.0) <= HEIGHT / 2.0)
                return 1.0;
        return 0.0;
}

static double trg3(double y2, void* _params) {
        our_integration_params params = *(our_integration_params*)_params;

        return src(params.x2 + params.x, y2 + params.y, params.slope)
                * lsf(sqrt(params.x2 * params.x2 + y2 * y2));
}

static double trg2(double x2, void* params) {
        gsl_function F;
        double result, abserror;
        
        ((our_integration_params*)params)->x2 = x2;

        F.params = params;
        F.function = &trg3;

        gsl_integration_qag(&F, -LSF_HEIGHT, LSF_HEIGHT, 0.001, 0, 1000,
                        GSL_INTEG_GAUSS61, w2, &result, &abserror);
        return result;
}

static double trg(double x, double y, double slope) {
        gsl_function F;
        double result, abserror;
        our_integration_params params = {slope, x, y, 0};
        
        if (y - slope * (x - WIDTH / 2.0) <= HEIGHT / 2.0 - LSF_HEIGHT)
                return 1.0;
        if (y - slope * (x - WIDTH / 2.0) >= HEIGHT / 2.0 + LSF_HEIGHT)
                return 0.0;

        F.params = &params;
        F.function = &trg2;
        gsl_integration_qag(&F, -LSF_WIDTH, LSF_WIDTH, 0.001, 0, 1000,
                        GSL_INTEG_GAUSS61, w1, &result, &abserror);
        result /= 0.004263590029716198;
        return result;
}

static int generate(double angle, char* filename) {
        double slope = tan(angle);
        unsigned int x, y;
        int ret = 0;

        FILE* png_fp;
        png_infop png_info_ptr = 0;
        png_structp png_ptr = 0;
        png_byte** png_rp = 0;

        png_fp = fopen(filename, "wb");
        if (!png_fp) {
                ret = -1;
                goto fopen_failed;
        }

        png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL,
                        NULL);
        if (png_ptr == NULL) {
                ret = -2;
                goto png_create_write_struct_failed;
        }

        png_info_ptr = png_create_info_struct(png_ptr);
        if (png_info_ptr == NULL) {
                ret = -3;
                goto png_create_info_struct_failed;
        }

        if (setjmp(png_jmpbuf(png_ptr))) {
                ret = -4;
                goto png_failure;
        }

        png_set_IHDR(png_ptr, png_info_ptr, WIDTH, HEIGHT, 8,
                        PNG_COLOR_TYPE_GRAY,
                        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                        PNG_FILTER_TYPE_DEFAULT);

        png_rp = png_malloc(png_ptr, HEIGHT * sizeof(png_byte*));
        for (y = 0; y < HEIGHT; y++)
                png_rp[y] = png_malloc(png_ptr, sizeof(uint8_t)*WIDTH);

        w1 = gsl_integration_workspace_alloc(1000);
        w2 = gsl_integration_workspace_alloc(1000);

        for (x = 0; x < WIDTH; x++) {
                if (x % 100 == 0) {
                        printf(" row %d\n", x);
                }
                for (y = 0; y < HEIGHT; y++) {
                        double v = trg((double)x, (double)y, slope) * 256;
                        uint8_t pv = 0;
                        if (v <= 0)
                                pv = 0;
                        else if (v >= 255)
                                pv = 255;
                        else
                                pv = (uint8_t)v;
                        png_rp[y][x] = pv;
                }
        }

        png_init_io(png_ptr, png_fp);
        png_set_rows(png_ptr, png_info_ptr, png_rp);
        png_write_png(png_ptr, png_info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

        for (y = 0; y < HEIGHT; y++)
                png_free(png_ptr, png_rp[y]);
        png_free(png_ptr, png_rp);

        gsl_integration_workspace_free(w1);
        gsl_integration_workspace_free(w2);

png_failure:
png_create_info_struct_failed:
png_create_write_struct_failed:
        fclose(png_fp);

fopen_failed:
        return ret;
}

int main(int argc, char** argv) {
        unsigned int i;
        int ret;
        char* filename;

        for (i = 1; i < argc; i++) {
                double angle;

                if(sscanf(argv[i], "%lf", &angle) != 1)
                        return -5;
                printf("%lf", angle);
                if(asprintf(&filename, "%.2lf.png", angle) == -1)
                        return -6;
                ret = generate(angle / 180.0 *M_PI, filename);
                free(filename);

                if (ret != 0)
                        return ret;
        }
        
        return 0;
}
