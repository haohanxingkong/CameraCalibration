#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/calib3d/calib3d.hpp>

using namespace cv;
using namespace std;
void clean_from_elipses(Mat &out, vector<Point2f> pattern_centers);
void update_mask_from_points(vector<Point2f> points, int w, int h, Point mask_point[][4]);

float average(vector<float> data)
{
    float sum = 0;
    for (int i = 0; i < data.size(); ++i)
        sum += data[i];

    return sum / data.size();
}

int find_points(Mat &src_gray, Mat &masked, Mat&original, int w, int h, Point mask_point[][4], vector<Point2f> &pattern_points, int &keep_per_frames) {

    Mat threshold_output;
    vector<vector<Point> > contours;
    vector<Vec4i> hierarchy;
    Point2f center, center_2;

    //Dilate
    int erosion_size = 2;
    Mat kernel = getStructuringElement( MORPH_ELLIPSE,
                                        Size( 2 * erosion_size + 1, 2 * erosion_size + 1 ),
                                        Point( erosion_size, erosion_size ) );

    //erode( src_gray, src_gray, kernel );

    morphologyEx(src_gray, src_gray, MORPH_CLOSE, kernel);

    /// Find contours // last was threshold_output
    findContours( src_gray, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0) );

    vector<RotatedRect>    ellipses_temp;
    vector<Point2f> new_pattern_points;
    float radio = 0;

    for (int contour = 0; contour < contours.size(); ++contour){
        if (contours[contour].size() > 4) {
            RotatedRect elipse  = fitEllipse( Mat(contours[contour]) );
            ellipse(masked, elipse, Scalar(255, 0, 255), 2);
            if (hierarchy[contour][2] != -1) { //si tiene un hijo

                int padre = contour;
                int hijo  = hierarchy[contour][2];

                if (contours[hijo].size() > 4 ) {
                    RotatedRect elipseHijo  = fitEllipse( Mat(contours[hijo]) );
                    RotatedRect elipsePadre = fitEllipse( Mat(contours[padre]) );
                    radio = (elipseHijo.size.height + elipseHijo.size.width) / 4;

                    if ( cv::norm(elipsePadre.center - elipseHijo.center) < radio / 2) { //CALIBRAR DE ACUERDO A VIDEO
                        ellipses_temp.push_back(elipsePadre);
                    }
                }
            }
        }
    }

    /**/
    int count = 0;
    float radio_prom = 0;
    for (int i = 0; i < ellipses_temp.size(); ++i) {
        count = 0;
        for (int j = 0; j < ellipses_temp.size(); ++j) {
            if (i == j) continue;
            radio = (ellipses_temp[j].size.height + ellipses_temp[j].size.width) / 4;
            float distance = cv::norm(ellipses_temp[i].center - ellipses_temp[j].center);
            if (distance < radio * 3.5) {
                line(masked, ellipses_temp[i].center, ellipses_temp[j].center, Scalar(0, 0, 255), 5);
                count++;
            }else{
                if(distance < radio * 4){
                    line(masked, ellipses_temp[i].center, ellipses_temp[j].center, Scalar(0, 255, 255), 2);
                    count++;
                }else{
                    if(distance < radio * 5){
                        line(masked, ellipses_temp[i].center, ellipses_temp[j].center, Scalar(0, 0, 0), 2);
                        count++;

                    }
                }

            }
        }
        if (count >= 2) {
            radio = (ellipses_temp[i].size.height + ellipses_temp[i].size.width) / 4;
            radio_prom += radio;
            new_pattern_points.push_back(ellipses_temp[i].center);
            circle(masked, ellipses_temp[i].center, radio, Scalar(0, 255, 0), 5);
        }
    }
    radio_prom /= new_pattern_points.size();
    
    if (new_pattern_points.size() == 20) {
        pattern_points.clear();
        for (int i = 0; i < new_pattern_points.size(); ++i) {
            int thickness = -1;
            int lineType = 8;
            circle( original,
                    new_pattern_points[i],
                    3,
                    Scalar( 0, 255, 0 ),
                    thickness,
                    lineType );
            pattern_points.push_back(new_pattern_points[i]);
        }
        keep_per_frames = 2;
        //drawChessboardCorners( original, Size(4,5), Mat(pattern_points),true );
    }

    if (keep_per_frames-- > 0) {
        clean_from_elipses(original, pattern_points);
    } else {
        pattern_points.clear();
    }
    update_mask_from_points(pattern_points, w, h, mask_point);
    return pattern_points.size();
}
float angle_between_two_points(Point2f p1, Point2f p2) {
    float angle = atan2(p1.y - p2.y, p1.x - p2.x);
    return angle * 180 / 3.1416;
}
float distance_to_rect(Point2f p1, Point2f p2, Point2f x) {
    float result = abs((p2.y - p1.y) * x.x - (p2.x - p1.x) * x.y + p2.x * p1.y - p2.y * p1.x) / sqrt(pow(p2.y - p1.y, 2) + pow(p2.x - p1.x, 2));
    return result;
}
vector<Point2f> more_distante_points(vector<Point2f>points) {
    float distance = 0;
    double temp;
    int p1, p2;
    for (int i = 0; i < points.size(); i++) {
        for (int j = 0; j < points.size(); j++) {
            if (i != j) {
                temp = cv::norm(points[i] - points[j]);
                if (distance < temp) {
                    distance = temp;
                    p1 = i;
                    p2 = j;
                }

            }
        }
    }
    if (points[p1].x < points[p2].x) {
        distance = p1;
        p1 = p2;
        p2 = distance;
    }
    vector<Point2f> p;
    p.push_back(points[p1]);
    p.push_back(points[p2]);
    return p;
}
void clean_from_elipses(Mat &drawing, vector<Point2f> pattern_centers) {
    if (pattern_centers.size() < 20) {
        return;
    }
    vector<Scalar> color_palette(5);
    color_palette[0] = Scalar(255, 0, 255);
    color_palette[1] = Scalar(255, 0, 0);
    color_palette[2] = Scalar(0, 255, 0);
    color_palette[3] = Scalar(0, 0 , 255);

    int coincidendes = 0;
    int centers = pattern_centers.size();
    float pattern_range = 5;
    vector<Point2f> temp;
    vector<Point2f> line_points;
    vector<Point2f> limit_points;
    int line_color = 0;
    for (int i = 0; i < centers; i++) {
        for (int j = 0; j < centers; j++) {
            if (i != j) {
                temp.clear();
                line_points.clear();
                coincidendes = 0;
                for (int k = 0; k < centers; k++) {
                    if (distance_to_rect(pattern_centers[i], pattern_centers[j], pattern_centers[k]) < pattern_range) {
                        coincidendes++;
                        line_points.push_back(pattern_centers[k]);
                    } else {
                        //temp.push_back(pattern_centers[k]);
                    }
                }

                if (coincidendes >= 5) {
                    line_points = more_distante_points(line_points);
                    bool found = false;
                    for (int l = 0; l < limit_points.size(); l++) {
                        if (limit_points[l].x == line_points[0].x && limit_points[l].y == line_points[0].y) {
                            found = true;
                        }
                    }
                    if (!found) {
                        limit_points.push_back(line_points[0]);
                        limit_points.push_back(line_points[1]);
                        if (line_color != 0) {
                            line(drawing, line_points[1], limit_points[line_color * 2 - 2], color_palette[line_color], 2);
                        }
                        line(drawing, line_points[0], line_points[1], color_palette[line_color], 2);
                        line_color++;
                    }
                }
            }
        }
    }
}
void update_mask_from_points(vector<Point2f> points, int w, int h, Point mask_point[][4]) {
    if (points.size() < 20) {
        mask_point[0][0]  = Point(0, 0);
        mask_point[0][1]  = Point(h, 0);
        mask_point[0][2]  = Point(h, w);
        mask_point[0][3]  = Point(0, w);
        return;
    }

    RotatedRect boundRect = minAreaRect( Mat(points) );
    Point2f rect_points[4];
    boundRect.points( rect_points );
    Mat original;

    double scale = 1.5;
    Point mask_center(( rect_points[0].x +
                        rect_points[1].x +
                        rect_points[2].x +
                        rect_points[3].x) / 4,
                      (rect_points[0].y +
                       rect_points[1].y +
                       rect_points[2].y +
                       rect_points[3].y) / 4);

    mask_point[0][0]  = Point((rect_points[0].x - mask_center.x) * scale + mask_center.x, (rect_points[0].y - mask_center.y) * scale + mask_center.y);
    mask_point[0][1]  = Point((rect_points[1].x - mask_center.x) * scale + mask_center.x, (rect_points[1].y - mask_center.y) * scale + mask_center.y);
    mask_point[0][2]  = Point((rect_points[2].x - mask_center.x) * scale + mask_center.x, (rect_points[2].y - mask_center.y) * scale + mask_center.y);
    mask_point[0][3]  = Point((rect_points[3].x - mask_center.x) * scale + mask_center.x, (rect_points[3].y - mask_center.y) * scale + mask_center.y);

}