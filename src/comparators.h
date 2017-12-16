class Comparator {
	protected:
		cv::Mat original;
	public:
		bool justChanged = false;

		Comparator(const Mat &o) {
			original = o;
		}

		cv::Size size() const {
			return getTarget().size();
		}

		virtual ~Comparator() {}

		virtual void setTarget(Mat &o) {
			original = o;
			justChanged = true;
		}

		const Mat &getTarget() const {
			return original;
		}

		virtual double diff(const Mat &other) const = 0;
};

class MSEComparator: public Comparator {
	public:
		MSEComparator(const Mat &o) : Comparator(o) {}

		double diff(const Mat &other) const override {
			Mat dst;

			absdiff(original, other, dst);
			dst.convertTo(dst, CV_16UC3);
			dst = dst.mul(dst);

			Scalar s = sum(dst);
			return s[0] + s[1] + s[2];
		}
};


class MSSIMComparator: public Comparator {
	public:
		MSSIMComparator(const Mat &o) : Comparator(o) {
		}

		double diff(const Mat &other) const {
			return getMSSIM(original, other);
		}

		/* Taken from https://docs.opencv.org/3.3.0/d5/dc4/tutorial_video_input_psnr_ssim.html */
		double getMSSIM(const Mat &i1, const Mat& i2) const {
			const double C1 = 6.5025, C2 = 58.5225;
			/***************************** INITS **********************************/
			int d = CV_32F;

			Mat I1, I2;
			i1.convertTo(I1, d);            // cannot calculate on one byte large values
			i2.convertTo(I2, d);

			Mat I2_2   = I2.mul(I2);        // I2^2
			Mat I1_2   = I1.mul(I1);        // I1^2
			Mat I1_I2  = I1.mul(I2);        // I1 * I2

			/*************************** END INITS **********************************/

			Mat mu1, mu2;                   // PRELIMINARY COMPUTING
			GaussianBlur(I1, mu1, Size(11, 11), 1.5);
			GaussianBlur(I2, mu2, Size(11, 11), 1.5);

			Mat mu1_2   =   mu1.mul(mu1);
			Mat mu2_2   =   mu2.mul(mu2);
			Mat mu1_mu2 =   mu1.mul(mu2);

			Mat sigma1_2, sigma2_2, sigma12;

			GaussianBlur(I1_2, sigma1_2, Size(11, 11), 1.5);
			sigma1_2 -= mu1_2;

			GaussianBlur(I2_2, sigma2_2, Size(11, 11), 1.5);
			sigma2_2 -= mu2_2;

			GaussianBlur(I1_I2, sigma12, Size(11, 11), 1.5);
			sigma12 -= mu1_mu2;

			///////////////////////////////// FORMULA ////////////////////////////////
			Mat t1, t2, t3;

			t1 = 2 * mu1_mu2 + C1;
			t2 = 2 * sigma12 + C2;
			t3 = t1.mul(t2);                 // t3 = ((2*mu1_mu2 + C1).*(2*sigma12 + C2))

			t1 = mu1_2 + mu2_2 + C1;
			t2 = sigma1_2 + sigma2_2 + C2;
			t1 = t1.mul(t2);                 // t1 =((mu1_2 + mu2_2 + C1).*(sigma1_2 + sigma2_2 + C2))

			Mat ssim_map;
			divide(t3, t1, ssim_map);        // ssim_map =  t3./t1;

			Scalar mssim = mean(ssim_map);   // mssim = average of ssim map
			return mssim[0] + mssim[1] + mssim[2];
		}
};
