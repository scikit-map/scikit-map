#include <iostream>
#include <fftw3.h>
#include <cmath>
#include <Eigen/Dense>

// Structure to have partial specializations of types for the FFTW library
template <class T>
struct TypesFFTW;

template <>
struct TypesFFTW<double> {
    using PlanType = fftw_plan;
    using ComplexType = fftw_complex;
    static constexpr auto Malloc = fftw_malloc;
    static constexpr auto PlanDFT_R2C = fftw_plan_dft_r2c_1d;
    static constexpr auto PlanDFT_C2R = fftw_plan_dft_c2r_1d;
    static constexpr auto Execute = fftw_execute;
    static constexpr auto ExecuteDFT_R2C = fftw_execute_dft_r2c;
    static constexpr auto ExecuteDFT_C2R = fftw_execute_dft_c2r;
    static constexpr auto DestroyPlan = fftw_destroy_plan;
    static constexpr auto Free = fftw_free;
};

template <>
struct TypesFFTW<float> {
    using PlanType = fftwf_plan;
    using ComplexType = fftwf_complex;
    static constexpr auto Malloc = fftwf_malloc;
    static constexpr auto PlanDFT_R2C = fftwf_plan_dft_r2c_1d;
    static constexpr auto PlanDFT_C2R = fftwf_plan_dft_c2r_1d;
    static constexpr auto Execute = fftwf_execute;
    static constexpr auto ExecuteDFT_R2C = fftwf_execute_dft_r2c;
    static constexpr auto ExecuteDFT_C2R = fftwf_execute_dft_c2r;
    static constexpr auto DestroyPlan = fftwf_destroy_plan;
    static constexpr auto Free = fftwf_free;
};


// Class to plan and perfoms all the FFT related operations
template <class T>
class WrapFFT {
private:
    
    using ComplexType = typename TypesFFTW<T>::ComplexType;
    using PlanType = typename TypesFFTW<T>::PlanType;
    unsigned int m_N_fft;
    unsigned int m_N_ext;
    unsigned int m_N_pix;
    T* m_in_forward;
    ComplexType* m_out_forward;
    ComplexType* m_in_backward;
    T* m_out_backward;
    PlanType m_plan_forward;
    PlanType m_plan_backward;
    T* m_ts_ext_data;
    T* m_mask_ext_data;    
    std::complex<T>* m_ts_ext_fft_data;
    std::complex<T>* m_mask_ext_fft_data;
    PlanType m_fftPlan_fw_ts;
    PlanType m_fftPlan_fw_mask;
    PlanType m_fftPlan_bw_conv_ts;
    PlanType m_fftPlan_bw_conv_mask;

    void computeMultipleFFT(PlanType fftPlan_fw, T* data, std::complex<T>* fft_data) {
        // Compute the forward transforms 
        for (unsigned int i = 0; i < m_N_pix; ++i) {
            TypesFFTW<T>::ExecuteDFT_R2C(fftPlan_fw, data + i * m_N_ext, reinterpret_cast<ComplexType*>(fft_data) + i * m_N_fft);
        }
    }

    void computeMultipleIFFT(PlanType fftPlan_bw, std::complex<T>* fft_data, T* data) {
        // Compute the backward transforms 
        for (unsigned int i = 0; i < m_N_pix; ++i) {
            TypesFFTW<T>::ExecuteDFT_C2R(fftPlan_bw, reinterpret_cast<ComplexType*>(fft_data) + i * m_N_fft, data + i * m_N_ext);
        }
    }

public:
    // Constructorfftw_plan_dft_r2c_1dm
    WrapFFT(unsigned int N_fft,
            unsigned int N_ext,
            unsigned int N_pix,
            T* ts_ext_data,
            T* mask_ext_data, 
            std::complex<T>* ts_ext_fft_data, 
            std::complex<T>* mask_ext_fft_data) {

        m_N_fft = N_fft;
        m_N_ext = N_ext;
        m_N_pix = N_pix;
        auto fftw_flags = FFTW_MEASURE;
        
        // Save the first row of the inputs since the planning could modify them
        T tmp_ts_ext_data[m_N_ext];
        T tmp_mask_ext_data[m_N_ext];
        for(unsigned int i = 0; i < m_N_ext; ++i) {
            tmp_ts_ext_data[i] = ts_ext_data[i];
            tmp_mask_ext_data[i] = mask_ext_data[i];
        }

        // Create input and output arrays
        m_ts_ext_data = ts_ext_data;
        m_mask_ext_data = mask_ext_data;    
        m_ts_ext_fft_data = reinterpret_cast<std::complex<T>*>(ts_ext_fft_data);
        m_mask_ext_fft_data = reinterpret_cast<std::complex<T>*>(mask_ext_fft_data);        
        m_in_forward = (T*)TypesFFTW<T>::Malloc(sizeof(T) * m_N_ext);
        m_out_forward = (ComplexType*)TypesFFTW<T>::Malloc(sizeof(ComplexType) * m_N_fft);
        m_in_backward = (ComplexType*)TypesFFTW<T>::Malloc(sizeof(ComplexType) * m_N_fft);
        m_out_backward = (T*)TypesFFTW<T>::Malloc(sizeof(T) * m_N_ext);

        // Create plans for forward and backward DFT (Discrete Fourier Transform)
        m_fftPlan_fw_ts = TypesFFTW<T>::PlanDFT_R2C(m_N_ext, m_ts_ext_data, reinterpret_cast<ComplexType*>(m_ts_ext_fft_data), fftw_flags);
        m_fftPlan_fw_mask = TypesFFTW<T>::PlanDFT_R2C(m_N_ext, m_mask_ext_data, reinterpret_cast<ComplexType*>(m_mask_ext_fft_data), fftw_flags);
        m_fftPlan_bw_conv_ts = TypesFFTW<T>::PlanDFT_C2R(m_N_ext, reinterpret_cast<ComplexType*>(m_ts_ext_fft_data), m_ts_ext_data, fftw_flags);
        m_fftPlan_bw_conv_mask = TypesFFTW<T>::PlanDFT_C2R(m_N_ext, reinterpret_cast<ComplexType*>(m_mask_ext_fft_data), m_mask_ext_data, fftw_flags);
        m_plan_forward = TypesFFTW<T>::PlanDFT_R2C(N_ext, m_in_forward, m_out_forward, fftw_flags);
        m_plan_backward = TypesFFTW<T>::PlanDFT_C2R(N_ext, m_in_backward, m_out_backward, fftw_flags);
        
        for(unsigned int i = 0; i < m_N_ext; ++i) {
            ts_ext_data[i] = tmp_ts_ext_data[i];
            mask_ext_data[i] = tmp_mask_ext_data[i];
        }
    }

    void computeFFT(const Eigen::Matrix<T, Eigen::Dynamic, 1>& vec_in, Eigen::Ref<Eigen::Matrix<std::complex<T>, Eigen::Dynamic, 1>> vec_out) {
        // Compute the forward transform
        for (unsigned int i = 0; i < m_N_ext; ++i) {
            m_in_forward[i] = vec_in(i);
        }
        TypesFFTW<T>::Execute(m_plan_forward);
        const std::complex<T> im(0.0, 1.0);
        for (unsigned int i = 0; i < m_N_fft; ++i) {
            vec_out(i) = m_out_forward[i][0] + m_out_forward[i][1]*im;
        }
    }

    void computeIFFT(const Eigen::Matrix<std::complex<T>, Eigen::Dynamic, 1>& vec_in, Eigen::Ref<Eigen::Matrix<T, Eigen::Dynamic, 1>> vec_out) {
        // Compute the backward transform
        for (unsigned int i = 0; i < m_N_fft; ++i) {
            m_in_backward[i][0] = vec_in[i].real()/m_N_ext;
            m_in_backward[i][1] = vec_in[i].imag()/m_N_ext;
        }
        TypesFFTW<T>::Execute(m_plan_backward);
        for (unsigned int i = 0; i < m_N_ext; ++i) {
            vec_out(i) = m_out_backward[i];
        }
    }

    void computeTimeserisFFT() {
        // Compute the forward transforms for the time series
        computeMultipleFFT(m_fftPlan_fw_ts, m_ts_ext_data, m_ts_ext_fft_data);
    }

    void computeMaskFFT() {
        // Compute the forward transforms for the time series
        computeMultipleFFT(m_fftPlan_fw_mask, m_mask_ext_data, m_mask_ext_fft_data);
    }

    void computeTimeserisIFFT() {
        // Compute the forward transforms for the time series
        computeMultipleIFFT(m_fftPlan_bw_conv_ts, m_ts_ext_fft_data, m_ts_ext_data);
    }

    void computeMaskIFFT() {
        // Compute the forward transforms for the time series
        computeMultipleIFFT(m_fftPlan_bw_conv_mask, m_mask_ext_fft_data, m_mask_ext_data);
    }

    void clean() {
         // Clean up
        TypesFFTW<T>::DestroyPlan(m_plan_forward);
        TypesFFTW<T>::Free(m_in_forward);
        TypesFFTW<T>::Free(m_out_forward);
        TypesFFTW<T>::DestroyPlan(m_plan_backward);
        TypesFFTW<T>::Free(m_in_backward);
        TypesFFTW<T>::Free(m_out_backward);
        TypesFFTW<T>::DestroyPlan(m_fftPlan_bw_conv_ts);
        TypesFFTW<T>::DestroyPlan(m_fftPlan_bw_conv_mask);
        TypesFFTW<T>::DestroyPlan(m_fftPlan_fw_ts);
        TypesFFTW<T>::DestroyPlan(m_fftPlan_fw_mask);
    }
};
