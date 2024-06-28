#ifndef LIBCA_UTILITY_TENSOR_HPP
#define LIBCA_UTILITY_TENSOR_HPP

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace ts
{
    template <typename T>
    class Tensor;

    // If your API does not match the above, you can use the following snippet to adapt
    // your API to the above. Specifically, you need to expose a class template named
    // {@code Tensor} accepting one type parameter {@code T}.

    /*
    template<typename T>
    class TensorImpl;  // change this forward declaration to your implementation

    template<typename T>
    using Tensor = TensorImpl<T>;
    */
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "readability-redundant-declaration"

namespace bm
{

    /**
     * @brief Creates a tensor from a given array by copying data to its own memory (the
     * tensor has no ownership of {@code data}).
     * @param shape shape of the tensor (size of each dimension)
     * @param data flatten data of the tensor (row major)
     * @note Please delegate a proper implemented {@code ts::Tensor} constructor to create
     * a tensor
     */

    template <typename T>
    ts::Tensor<T> create_with_data(const std::vector<size_t> & shape, const T * data);

    template <typename T>
    ts::Tensor<T> rand(const std::vector<size_t> & shape);

    template <typename T>
    ts::Tensor<T> zeros(const std::vector<size_t> & shape);

    template <typename T>
    ts::Tensor<T> ones(const std::vector<size_t> & shape);

    template <typename T>
    ts::Tensor<T> full(const std::vector<size_t> & shape, const T & value);

    template <typename T>
    ts::Tensor<T> eye(size_t rows, size_t cols);

    template <typename T>
    ts::Tensor<T> concat(const std::vector<ts::Tensor<T>> & tensors, size_t axis);

    template <typename T>
    ts::Tensor<T> tile(const ts::Tensor<T> & tensor, const std::vector<size_t> & shape);

    template <typename T>
    ts::Tensor<T> transpose(const ts::Tensor<T> & tensor, size_t dim1, size_t dim2);

    template <typename T>
    ts::Tensor<T> permute(const ts::Tensor<T> & tensor,
                          const std::vector<size_t> & permutation);

    /**
     * @brief Returns the element at the given position.
     * @param tensor tensor to access
     * @param indices indices of the element, guaranteed to be a vector of size {@code
     * tensor.dim()}
     * @return reference to the element at the given position
     */
    template <typename T>
    T at(const ts::Tensor<T> & tensor, const std::vector<size_t> & indices);

    template <typename T>
    ts::Tensor<T> slice(const ts::Tensor<T> & tensor,
                        const std::vector<std::pair<size_t, size_t>> & slices);

    template <typename T>
    ts::Tensor<T> einsum(const std::string & equation,
                         const std::vector<ts::Tensor<T>> & tensors);

    template <typename T>
    ts::Tensor<T> view(const ts::Tensor<T> & tensor, const std::vector<size_t> & shape);

    template <typename T>
    void
    set_at(ts::Tensor<T> & tensor, const std::vector<size_t> & indices, const T & value);

    template <typename T>
    ts::Tensor<T> pointwise_add(ts::Tensor<T> & a, ts::Tensor<T> & b);

    template <typename T>
    ts::Tensor<T> pointwise_sub(ts::Tensor<T> & a, ts::Tensor<T> & b);

    template <typename T>
    ts::Tensor<T> pointwise_mul(ts::Tensor<T> & a, ts::Tensor<T> & b);

    template <typename T>
    ts::Tensor<T> pointwise_div(ts::Tensor<T> & a, ts::Tensor<T> & b);

    template <typename T>
    ts::Tensor<T> pointwise_log(ts::Tensor<T> & tensor);

    template <typename T>
    ts::Tensor<T> reduce_sum(const ts::Tensor<T> & tensor, size_t axis);

    template <typename T>
    ts::Tensor<T> reduce_mean(const ts::Tensor<T> & tensor, size_t axis);

    template <typename T>
    ts::Tensor<T> reduce_max(const ts::Tensor<T> & tensor, size_t axis);

    template <typename T>
    ts::Tensor<T> reduce_min(const ts::Tensor<T> & tensor, size_t axis);

    template <typename T>
    ts::Tensor<bool> eq(const ts::Tensor<T> & a, const ts::Tensor<T> & b);

    template <typename T>
    ts::Tensor<bool> ne(const ts::Tensor<T> & a, const ts::Tensor<T> & b);

    template <typename T>
    ts::Tensor<bool> gt(const ts::Tensor<T> & a, const ts::Tensor<T> & b);

    template <typename T>
    ts::Tensor<bool> ge(const ts::Tensor<T> & a, const ts::Tensor<T> & b);

    template <typename T>
    ts::Tensor<bool> lt(const ts::Tensor<T> & a, const ts::Tensor<T> & b);

    template <typename T>
    ts::Tensor<bool> le(const ts::Tensor<T> & a, const ts::Tensor<T> & b);

    template <typename T>
    std::vector<T> returnTimes(const std::vector<T> & data, const int & times);

    template <typename T>
    std::vector<T> addVector(const std::vector<T> & a, const std::vector<T> & b);
}

#pragma clang diagnostic pop

// include your implementation's header file here, e.g.

#include <fstream>
#include <iostream>
#include <random>

// 4*5*5
namespace ts
{
    template <typename T>
    class Tensor
    {
    public:
        std::vector<size_t> shape;   // 4 5 5
        std::vector<size_t> strides; // 25 5 1
        std::vector<T> originalData;
        size_t dim{};

        // 1 1 1
        // 3 3 3
        // 9 9 9

        std::vector<Tensor<T>> dataHistory; //用于自动求导的历史数据

        // 使用中心差分公式计算梯度
        Tensor<T>
        numericalGradient(double h)
        {
            Tensor<T> gradient(shape, std::vector<T>(originalData.size(), 0));
            size_t totalSize = 1;
            for (size_t i = 0; i < shape.size(); ++i) {
                totalSize *= shape[i];
            }
            for (size_t i = 0; i < originalData.size(); ++i) {
                for (size_t j = 0; j < shape.size(); ++j) {
                    size_t index = i * strides[j] + j;
                    if (index < originalData.size()) {
                        // 使用中心差分公式计算梯度
                        size_t left = index - shape[j] * h;
                        size_t right = index + shape[j] * h;
                        if (left < 0 || right >= originalData.size()) {
                            // 边界处理
                            gradient.originalData[i] += originalData[index] * h;
                        }
                        else {
                            gradient.originalData[i] +=
                              (originalData[left] - originalData[right]) / (2 * h);
                        }
                    }
                }
            }
            return gradient;
        }

    public:
        Tensor()
        {
            this->shape = {};
            this->dim = 0;
            this->originalData = {};
            init_strides();
        }

        Tensor(const std::vector<size_t> & shape, const T * data)
        {
            this->shape = shape;
            this->dim = shape.size();
            int size = 1;
            for (int i = 0; i < shape.size(); i++) {
                size *= shape[i];
            }
            for (int i = 0; i < size; i++) {
                this->originalData.push_back(data[i]);
            }
            init_strides();
        }

        Tensor(const std::vector<size_t> & shape, const std::vector<T> & data)
        {
            this->shape = shape;
            this->dim = shape.size();
            this->originalData = data;
            init_strides();
        }

        Tensor(const int & data)
        {
            this->shape = { 1 };
            this->dim = 1;
            this->originalData = { data };
            init_strides();
        }

        Tensor(const float & data)
        {
            this->shape = { 1 };
            this->dim = 1;
            this->originalData = { data };
            init_strides();
        }

        Tensor(const double & data)
        {
            this->shape = { 1 };
            this->dim = 1;
            this->originalData = { data };
            init_strides();
        }

        bool
        readFile(const std::string & fileName)
        {
            std::ifstream file;
            file.open(fileName, std::ios::in);
            if (!file.is_open()) {
                std::cout << "open file error" << std::endl;
                return false;
            }
            //第一行为shape，第二行为strides，第三行为data，第四行为历史数据的行数，接下来为历史数据，均为空格分隔
            std::string tmp;
            std::getline(file, tmp);
            std::vector<size_t> shape;
            std::vector<size_t> strides;
            std::vector<T> data;
            std::vector<Tensor<T>> dataHistory;
            std::string tmp2;
            for (int i = 0; i < tmp.size(); i++) {
                if (tmp[i] == ' ') {
                    shape.push_back(std::stoi(tmp2));
                    tmp2 = "";
                    continue;
                }
                tmp2 += tmp[i];
            }
            std::getline(file, tmp);
            tmp2 = "";
            for (int i = 0; i < tmp.size(); i++) {
                if (tmp[i] == ' ') {
                    strides.push_back(std::stoi(tmp2));
                    tmp2 = "";
                    continue;
                }
                tmp2 += tmp[i];
            }
            std::getline(file, tmp);
            tmp2 = "";
            for (int i = 0; i < tmp.size(); i++) {
                if (tmp[i] == ' ') {
                    data.push_back(std::stoi(tmp2));
                    tmp2 = "";
                    continue;
                }
                tmp2 += tmp[i];
            }
            std::getline(file, tmp);
            int historySize = std::stoi(tmp);
            for (int i = 0; i < historySize; i++) {
                std::getline(file, tmp);
                tmp2 = "";
                std::vector<T> tmpData;
                for (int j = 0; j < tmp.size(); j++) {
                    if (tmp[j] == ' ') {
                        tmpData.push_back(std::stoi(tmp2));
                        tmp2 = "";
                        continue;
                    }
                    tmp2 += tmp[j];
                }
                dataHistory.push_back(ts::Tensor<T>(shape, tmpData));
            }
            file.close();
            this->shape = shape;
            this->strides = strides;
            this->originalData = data;
            this->dataHistory = dataHistory;
            this->dim = shape.size();
            return true;
        }

        bool
        writeFile(std::string fileName)
        {
            std::ofstream file;
            file.open(fileName, std::ios::out);
            if (!file.is_open()) {
                std::cout << "open file error" << std::endl;
                return false;
            }
            //第一行为shape，第二行为strides，第三行为data，第四行为历史数据的行数，接下来为历史数据，均为空格分隔
            std::string tmp;
            for (int i = 0; i < shape.size(); i++) {
                tmp += std::to_string(shape[i]) + " ";
            }
            file << tmp << std::endl;
            tmp = "";
            for (int i = 0; i < strides.size(); i++) {
                tmp += std::to_string(strides[i]) + " ";
            }
            file << tmp << std::endl;
            tmp = "";
            for (int i = 0; i < originalData.size(); i++) {
                tmp += std::to_string(originalData[i]) + " ";
            }
            file << tmp << std::endl;
            tmp = "";
            file << dataHistory.size() << std::endl;
            for (int i = 0; i < dataHistory.size(); i++) {
                for (int j = 0; j < dataHistory[i].originalData.size(); j++) {
                    tmp += std::to_string(dataHistory[i].originalData[j]) + " ";
                }
                file << tmp << std::endl;
                tmp = "";
            }
            file.close();
            return true;
        }

        void
        init_strides()
        {
            int size = 1;
            for (int i = 0; i < shape.size(); i++) {
                size *= shape[i];
            }
            for (int i = 0; i < shape.size(); i++) {
                int tmp = 1;
                for (int j = i + 1; j < shape.size(); j++) {
                    tmp *= shape[j];
                }
                this->strides.push_back(tmp);
            }
        }

        //重载索引运算符 (index)
        T
        operator()(const std::vector<size_t> & indices) const
        {
            return bm::at(*this, indices);
        }

        //重载索引运算符 以 (1，2,{2,3}) 的形式调用
        Tensor<T> &
        operator()(const std::vector<std::pair<size_t, size_t>> & slices)
        {
            return bm::slice(*this, slices);
        }

        void
        show()
        {
            std::cout << "show tensor:" << std::endl;
            //使用at来遍历
            int size = 1;
            for (int i = 0; i < shape.size(); i++) {
                size *= shape[i];
            }
            for (int i = 0; i < size; i++) {
                std::vector<size_t> index;
                size_t tmp = i, step = size;
                for (int j = 0; j < shape.size(); j++) {
                    step /= shape[j];
                    index.push_back(tmp / step);
                    tmp %= step;
                }
                std::cout << bm::at(*this, index) << " ";
            }
            std::cout << std::endl;
        }

        //重载赋值运算符
        Tensor<T> &
        operator=(const Tensor<T> & tensor)
        {
            this->shape = tensor.shape;
            this->strides = tensor.strides;
            this->originalData = tensor.originalData;
            this->dim = tensor.dim;
            this->dataHistory = tensor.dataHistory;
            return *this;
        }
        Tensor<T> &
        operator=(const T & data)
        {
            this->shape = { 1 };
            this->strides = { 1 };
            this->originalData = { data };
            this->dim = 1;
            this->dataHistory = {};
            return *this;
        }
    };
}

namespace bm
{

    template <typename T>
    ts::Tensor<T>
    einsum(const std::string & equation, const std::vector<ts::Tensor<T>> & tensors)
    {
        //解析equation
        //以->为分隔符
        std::string left;
        std::string right;
        bool flag = false;
        for (int i = 0; i < equation.size(); i++) {
            if (equation[i] == '-') {
                flag = true;
                continue;
            }
            if (flag) {
                right += equation[i];
            }
            else {
                left += equation[i];
            }
        }
        right.erase(0, 1);
        //解析left
        std::vector<std::string> leftTensors;
        std::string tmp;
        for (int i = 0; i < left.size(); i++) {
            if (left[i] == ',') {
                leftTensors.push_back(tmp);
                tmp = "";
                continue;
            }
            tmp += left[i];
        }
        leftTensors.push_back(tmp);
        //如果left和right均只有一个
        if (leftTensors.size() == 1 && right.size() == 1) {
            //进一步判断
            bool flag = false;
            for (int i = 0; i < leftTensors[0].size(); i++) {
                if (leftTensors[0][i] != right[0]) {
                    flag = true;
                    break;
                }
            }
            // TODO  第五个einsum实现
            if (flag) {
                //有不相等的元素,以不相等的元素为轴进行求和
                //找到不相等的元素
                int axis = 0;
                for (int i = 0; i < leftTensors[0].size(); i++) {
                    if (leftTensors[0][i] == right[0]) {
                        axis = i;
                        break;
                    }
                }
                std::cout << "the fifth einsum" << std::endl;
                return reduce_sum(tensors[0], axis);
            }
            else {
                // TODO 第一个einsum实现
                //相等，返回对角线元素
                //输入一定合法
                //如果为一维
                if (tensors[0].shape.size() == 1) {
                    std::cout << "the first einsum" << std::endl;
                    return ts::Tensor<T>({ 1 }, { tensors[0].originalData[0] });
                }
                else {
                    //多维
                    //返回对角线元素
                    std::vector<T> data;
                    int size = tensors[0].shape.size();
                    for (int i = 0; i < size; i++) {
                        std::vector<size_t> diagonal(size, i);
                        data.push_back(bm::at(tensors[0], diagonal));
                    }
                    std::cout << "the first einsum" << std::endl;
                    return ts::Tensor<T>({ size }, data);
                }
            }
        }
        else if (leftTensors.size() == 1 && right.size() > 1) {
            // TODO 第二个+第三个einsum实现

            // transpose
            //找到不相等的元素
            std::vector<size_t> permutation;
            //初始化permutation
            for (int i = 0; i < leftTensors[0].size(); i++) {
                permutation.push_back(i);
            }
            std::vector<size_t> tmp;
            //找到相等的元素赋值
            for (int i = 0; i < right.size(); i++) {
                for (int j = 0; j < leftTensors[0].size(); j++) {
                    if (right[i] == leftTensors[0][j]) {
                        tmp.push_back(j);
                        break;
                    }
                }
            }
            std::cout << "the second and third einsum" << std::endl;
            return permute(tensors[0], tmp);
        }
        else if (right.size() == 0) {
            // TODO 第八个+第九个einsum实现
            //点乘
            auto resultTensor = tensors;
            if (leftTensors.size() >= 2) {
                for (int i = 0; i < tensors.size() - 1; i++) {
                    resultTensor[i + 1] =
                      pointwise_mul(resultTensor[i], resultTensor[i + 1]);
                }
                //再全部加起来
                std::cout << "the eighth and ninth einsum" << std::endl;
                std::vector<T> data;
                T sum = 0;
                for (int i = 0;
                     i < resultTensor[resultTensor.size() - 1].originalData.size();
                     i++) {
                    sum += resultTensor[resultTensor.size() - 1].originalData[i];
                }
                data.push_back(sum);
                return ts::Tensor<T>({ 1 }, data);
            }
            else {
                // TODO 第四个einsum实现
                //将所有元素加起来，返回单个元素
                std::vector<T> data;
                T sum = 0;
                for (int i = 0; i < tensors[0].originalData.size(); i++) {
                    sum += tensors[0].originalData[i];
                }
                data.push_back(sum);
                std::cout << "the fourth einsum" << std::endl;
                return ts::Tensor<T>({ 1 }, data);
            }
            //求和
            std::cout << "the eighth and ninth einsum" << std::endl;
            return reduce_sum(tensors[tensors.size() - 1], 0);
        }
        else if (leftTensors.size() >= 2) {
            // TODO 第十个einsum实现
            //如果左侧只有两个字符串且大小均为1，则为向量乘向量
            if (leftTensors.size() == 2 && leftTensors[0].size() == 1
                && leftTensors[1].size() == 1 && right.size() == 2) {
                //向量乘向量
                std::vector<T> data;
                //两个for循环计算最终的结果
                for (int i = 0; i < tensors[0].shape[0]; i++) {
                    for (int j = 0; j < tensors[1].shape[0]; j++) {
                        data.push_back(tensors[0].originalData[i]
                                       * tensors[1].originalData[j]);
                    }
                }
                std::vector<size_t> shape;
                shape.push_back(tensors[0].shape[0]);
                shape.push_back(tensors[1].shape[0]);
                std::cout << "the tenth einsum" << std::endl;
                return ts::Tensor<T>(shape, data);
            } //如果左侧只有两个字符串且其中一个大小为1，另一个为2，则为矩阵乘向量
              // TODO 第六个einsum实现
            else if (leftTensors.size() == 2 && leftTensors[0].size() == 1
                     && leftTensors[1].size() == 2 && right.size() == 1) {
                //矩阵乘向量
                std::vector<T> data;
                //两个for循环计算最终的结果
                for (int i = 0; i < tensors[1].shape[0]; i++) {
                    T sum = 0;
                    for (int j = 0; j < tensors[1].shape[1]; j++) {
                        sum += tensors[1].originalData[i * tensors[1].shape[1] + j]
                               * tensors[0].originalData[j];
                    }
                    data.push_back(sum);
                }
                std::vector<size_t> shape;
                shape.push_back(tensors[1].shape[0]);
                std::cout << "the sixth einsum" << std::endl;
                return ts::Tensor<T>(shape, data);
            } //如果左侧只有两个字符串且其中一个大小为2，另一个为1，则为向量乘矩阵
            else if (leftTensors.size() == 2 && leftTensors[0].size() == 2
                     && leftTensors[1].size() == 1 && right.size() == 1) {
                //向量乘矩阵
                std::vector<T> data;
                //两个for循环计算最终的结果
                for (int i = 0; i < tensors[0].shape[0]; i++) {
                    T sum = 0;
                    for (int j = 0; j < tensors[0].shape[1]; j++) {
                        sum += tensors[0].originalData[i * tensors[0].shape[1] + j]
                               * tensors[1].originalData[j];
                    }
                    data.push_back(sum);
                }
                std::vector<size_t> shape;
                shape.push_back(tensors[0].shape[0]);
                std::cout << "the sixth einsum" << std::endl;
                return ts::Tensor<T>(shape, data);
            } //如果左侧只有两个字符串且大小均为2，则为矩阵乘矩阵
              // TODO 第七个einsum实现
            else if (leftTensors.size() == 2 && leftTensors[0].size() == 2
                     && leftTensors[1].size() == 2 && right.size() == 2) {
                //矩阵乘矩阵
                std::vector<T> data;
                //两个for循环计算最终的结果
                for (int i = 0; i < tensors[0].shape[0]; i++) {
                    for (int j = 0; j < tensors[1].shape[1]; j++) {
                        T sum = 0;
                        for (int k = 0; k < tensors[0].shape[1]; k++) {
                            sum += tensors[0].originalData[i * tensors[0].shape[1] + k]
                                   * tensors[1].originalData[k * tensors[1].shape[1] + j];
                        }
                        data.push_back(sum);
                    }
                }
                std::vector<size_t> shape;
                shape.push_back(tensors[0].shape[0]);
                shape.push_back(tensors[1].shape[1]);
                std::cout << "the seventh einsum" << std::endl;
                return ts::Tensor<T>(shape, data);
            }
        }
        return tensors[0];
    }

    template <typename T>
    ts::Tensor<T>
    view(const ts::Tensor<T> & tensor, const std::vector<size_t> & shape)
    {
        ts::Tensor<T> result = tensor;
        result.shape = shape;
        result.init_strides();
        return result;
    }

    template <typename T>
    ts::Tensor<T>
    create_with_data(const std::vector<size_t> & shape, const T * data)
    {
        return ts::Tensor<T>(shape, data);
    }

    template <typename T>
    ts::Tensor<T>
    rand(const std::vector<size_t> & shape)
    {
        //生成0-1之间的随机数
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0, 1);
        std::vector<T> data;
        int size = 1;
        for (int i = 0; i < shape.size(); i++) {
            size *= shape[i];
        }
        for (int i = 0; i < size; i++) {
            data.push_back(dis(gen));
        }
        auto result = ts::Tensor<T>(shape, data);
        return result;
    }

    template <typename T>
    ts::Tensor<T>
    zeros(const std::vector<size_t> & shape)
    {
        //生成0
        std::vector<T> data;
        int size = 1;
        for (int i = 0; i < shape.size(); i++) {
            size *= shape[i];
        }
        for (int i = 0; i < size; i++) {
            data.push_back(T(0));
        }
        return ts::Tensor<T>(shape, data);
    }

    template <typename T>
    ts::Tensor<T>
    ones(const std::vector<size_t> & shape)
    {
        //生成1
        std::vector<T> data;
        int size = 1;
        for (int i = 0; i < shape.size(); i++) {
            size *= shape[i];
        }
        for (int i = 0; i < size; i++) {
            data.push_back(T(1));
        }
        return ts::Tensor<T>(shape, data);
    }

    template <typename T>
    ts::Tensor<T>
    full(const std::vector<size_t> & shape, const T & value)
    {
        //生成value
        std::vector<T> data;
        int size = 1;
        for (int i = 0; i < shape.size(); i++) {
            size *= shape[i];
        }
        for (int i = 0; i < size; i++) {
            data.push_back(T(value));
        }
        return ts::Tensor<T>(shape, data);
    }

    template <typename T>
    ts::Tensor<T>
    eye(size_t rows, size_t cols)
    {
        //生成单位矩阵
        std::vector<T> data;
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; ++j) {
                if (i == j) {
                    data.push_back(T(1));
                }
                else {
                    data.push_back(T(0));
                }
            }
        }
        return ts::Tensor<T>({ rows, cols }, data);
    }

    // TODO 此处为specificPatterns的实现
    template <typename T>
    ts::Tensor<T>
    specificPatterns(const ts::Tensor<T> & tensor,
                     const std::vector<size_t> & shape,
                     const int & pattern)
    {
        // pattern若为0，则给tensor添加正态分布的随机数
        if (pattern == 0) {
            std::vector<T> data;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::normal_distribution<> dis(0, 1);
            int size = 1;
            for (int i = 0; i < shape.size(); i++) {
                size *= shape[i];
            }
            for (int i = 0; i < size; i++) {
                data.push_back(tensor.originalData[i] + dis(gen));
            }
            return ts::Tensor<T>(shape, data);
        }
    }

    template <typename T>
    ts::Tensor<T> &
    slice(const ts::Tensor<T> & tensor,
          const std::vector<std::pair<size_t, size_t>> & slices)
    {
        //返回一个tensor
        //按照slices中的pair对tensor进行切片
        // pair中的first为起始位置，second为结束位置
        //适应所有维度，必须使用递归

        if (slices.size() == 1) {
            //只有一维
            std::vector<T> data;
            for (int i = slices[0].first; i < slices[0].second; i++) {
                data.push_back(tensor.originalData[i]);
            }
            return ts::Tensor<T>({ slices[0].second - slices[0].first }, data);
        }
        else {
            //多维
            //根据slices第一个pair对tensor进行切片
            std::vector<T> data;
            int size = tensor.originalData.size() / tensor.shape[0];
            for (int i = slices[0].first; i < slices[0].second; i++) {
                for (int j = 0; j < size; j++) {
                    data.push_back(tensor.originalData[i * size + j]);
                }
            }
            //删除slices中的第一个pair
            std::vector<std::pair<size_t, size_t>> tmp = slices;
            tmp.erase(tmp.begin());
            //删除tensor中的第一个维度
            std::vector<size_t> tmpShape = tensor.shape;
            tmpShape.erase(tmpShape.begin());
            //递归
            auto tmpTensor = ts::Tensor<T>(tmpShape, data);
            return slice(tmpTensor, tmp);
        }
    }

    template <typename T>
    ts::Tensor<T>
    concat(const std::vector<ts::Tensor<T>> & tensors, size_t axis)
    {
        std::vector<std::vector<T>> finalData;
        ts::Tensor<T> tensor = tensors[0];

        std::vector<T> data;
        std::vector<std::vector<T>> sliceData;
        sliceData.push_back(tensor.originalData);
        std::vector<std::vector<T>> tmpData;
        //递归到axis这个维度为止，前面的不断切片
        for (int i = 0; i < axis; i++) {
            //对sliceData中的每一个元素进行切片
            for (int j = 0; j < sliceData.size(); j++) {
                //获取其中一个元素的长度
                int size = sliceData[0].size();
                size = size / tensor.shape[i];
                //将元素切片为size个
                for (int k = 0; k < tensor.shape[i]; k++) {
                    std::vector<T> tmp;
                    for (int l = 0; l < size; l++) {
                        tmp.push_back(sliceData[j][k * size + l]);
                    }
                    tmpData.push_back(tmp);
                }
            }
            sliceData.clear();
            sliceData = tmpData;
            tmpData.clear();
        }

        finalData = sliceData;

        bool f = true;
        for (auto tensor : tensors) {
            if (f == true) {
                f = false;
                continue;
            }
            std::vector<T> data;
            std::vector<std::vector<T>> sliceData;
            sliceData.push_back(tensor.originalData);
            std::vector<std::vector<T>> tmpData;
            //递归到axis这个维度为止，前面的不断切片
            for (int i = 0; i < axis; i++) {
                //对sliceData中的每一个元素进行切片
                for (int j = 0; j < sliceData.size(); j++) {
                    //获取其中一个元素的长度
                    int size = sliceData[0].size();
                    size = size / tensor.shape[i];
                    //将元素切片为size个
                    for (int k = 0; k < tensor.shape[i]; k++) {
                        std::vector<T> tmp;
                        for (int l = 0; l < size; l++) {
                            tmp.push_back(sliceData[j][k * size + l]);
                        }
                        tmpData.push_back(tmp);
                    }
                }
                sliceData.clear();
                sliceData = tmpData;
                tmpData.clear();
            }

            for (int i = 0; i < finalData.size(); i++) {
                for (int j = 0; j < sliceData[0].size(); j++) {
                    finalData[i].push_back(sliceData[i][j]);
                }
            }
            std::cout << "concat" << std::endl;
        }

        std::vector<size_t> finalShape;
        for (int i = 0; i < tensor.shape.size(); i++) {
            finalShape.push_back(tensor.shape[i]);
        }
        finalShape[axis] = finalData[0].size();
        std::vector<T> finalData2;
        for (int i = 0; i < finalData.size(); i++) {
            for (int j = 0; j < finalData[0].size(); j++) {
                finalData2.push_back(finalData[i][j]);
            }
        }
        auto result = ts::Tensor<T>(finalShape, finalData2);
        return result;
    }

    template <typename T>

    ts::Tensor<T>
    tile(const ts::Tensor<T> & tensor, const std::vector<size_t> & shape)
    {
        std::vector<T> finalData;
        if (tensor.shape.size() == 1) {
            //一维
            for (int i = 0; i < shape[0]; i++) {
                for (int j = 0; j < tensor.shape[0]; j++) {
                    finalData.push_back(tensor.originalData[j]);
                }
            }
            return ts::Tensor<T>(shape, finalData);
        }
        else {
            //降维
            //删除tensor和shape中的第一个元素
            std::vector<size_t> tensorShape = tensor.shape;
            std::vector<size_t> shapeShape = shape;
            tensorShape.erase(tensorShape.begin());
            shapeShape.erase(shapeShape.begin());

            int size = tensor.originalData.size();
            size = size / tensor.shape[0];

            for (int j = 0; j < tensor.shape[0]; j++) {
                std::vector<T> tmp;
                for (int k = 0; k < size; k++) {
                    tmp.push_back(tensor.originalData[j * size + k]);
                }
                auto tmpTensor = ts::Tensor<T>(tensorShape, tmp);
                auto tmpTensor2 = tile(tmpTensor, shapeShape);
                for (int i = 0; i < tmpTensor2.originalData.size(); i++) {
                    finalData.push_back(tmpTensor2.originalData[i]);
                }
            }
            std::vector<T> result;
            for (int i = 0; i < shape[0]; i++) {
                for (int j = 0; j < finalData.size(); j++) {
                    result.push_back(finalData[j]);
                }
            }
            std::vector<size_t> finalShape;
            for (int i = 0; i < tensor.shape.size(); i++) {
                finalShape.push_back(shape[i] * tensor.shape[i]);
            }
            return ts::Tensor<T>(finalShape, result);
        }
    }

    // 1 2 3 4 5 6 7
    // 2 1 5 6 3 4 7
    template <typename T>
    ts::Tensor<T>
    transpose(const ts::Tensor<T> & tensor, size_t dim1, size_t dim2)
    {
        ts::Tensor<T> result = tensor;
        std::swap(result.shape[dim1], result.shape[dim2]);
        std::swap(result.strides[dim1], result.strides[dim2]);
        return result;
    }

    // 1 2 3 4 5 6 7
    template <typename T> // 2 1 5 6 3 4 7
    ts::Tensor<T>
    permute(const ts::Tensor<T> & tensor, const std::vector<size_t> & permutation)
    {
        ts::Tensor<T> result = tensor;
        std::vector<size_t> tmpShape;
        std::vector<size_t> tmpStrides;
        for (int i = 0; i < permutation.size(); i++) {
            tmpShape.push_back(result.shape[permutation[i]]);
            tmpStrides.push_back(result.strides[permutation[i]]);
        }
        result.shape = tmpShape;
        result.strides = tmpStrides;
        return result;
    }

    template <typename T>
    T
    at(const ts::Tensor<T> & tensor, const std::vector<size_t> & indices)
    {
        //使用strides
        int size = 0;
        for (int i = 0; i < tensor.shape.size(); i++) {
            size += tensor.strides[i] * indices[i];
        }
        return tensor.originalData[size];
    }

    template <typename T>
    void
    set_at(ts::Tensor<T> & tensor, const std::vector<size_t> & indices, const T & value)
    {
        //使用strides
        int size = 0;
        for (int i = 0; i < tensor.shape.size(); i++) {
            size += tensor.strides[i] * indices[i];
        }
        tensor.originalData[size] = value;
    }

    template <typename T>
    ts::Tensor<T>
    pointwise_add(ts::Tensor<T> & a, ts::Tensor<T> & b)
    {
        std::cout << "pointwise_add" << std::endl;

        ts::Tensor<T> left = a;
        ts::Tensor<T> right = b;
        //从后向前判断，如果某一个维度小于另一个维度，则将小的维度扩展到大的维度
        //但是维度小的必须部分形状相似
        auto size = a.shape.size() > b.shape.size() ? b.shape.size() : a.shape.size();
        bool which = a.shape.size() > b.shape.size();
        bool flag = true;
        for (int i = 0; i < size; i++) {
            if (a.shape[a.shape.size() - 1 - i] != b.shape[b.shape.size() - 1 - i]) {
                flag = false;
                break;
            }
        }
        if (flag) {
            std::cout << "boardcasting" << std::endl;
            if (which) {
                // b的维度小
                ts::Tensor<T> tmp = b;
                //将b的维度扩展到a的维度，向前插入1
                for (int i = 0; i < a.shape.size() - b.shape.size(); i++) {
                    tmp.shape.insert(tmp.shape.begin(), 1);
                }
                tmp.init_strides();
                // tile
                std::vector<size_t> data;
                for (int i = 0; i < a.shape.size(); i++) {
                    data.push_back(a.shape[i] / tmp.shape[i]);
                }
                auto tmp2 = tile(tmp, data);
                right = tmp2;
            }
            else {
                // a的维度小
                ts::Tensor<T> tmp = a;
                //将a的维度扩展到b的维度，向前插入1
                for (int i = 0; i < b.shape.size() - a.shape.size(); i++) {
                    tmp.shape.insert(tmp.shape.begin(), 1);
                }
                tmp.init_strides();
                // tile
                std::vector<size_t> data;
                for (int i = 0; i < b.shape.size(); i++) {
                    data.push_back(b.shape[i] / tmp.shape[i]);
                }
                auto tmp2 = tile(tmp, data);
                left = tmp2;
            }
        }
        else {
            std::cout << "shape error" << std::endl;
            return ts::Tensor<T>();
        }

        std::vector<T> data;
        for (int i = 0; i < left.originalData.size(); i++) {
            data.push_back(left.originalData[i] + right.originalData[i]);
        }
        ts::Tensor<T> history = ts::Tensor<T>(left.shape, data);
        a.dataHistory.push_back(history);
        b.dataHistory.push_back(history);
        return history;
    }

    template <typename T>
    ts::Tensor<T>
    pointwise_sub(ts::Tensor<T> & a, ts::Tensor<T> & b)
    {
        std::cout << "pointwise_sub" << std::endl;

        ts::Tensor<T> left = a;
        ts::Tensor<T> right = b;
        //从后向前判断，如果某一个维度小于另一个维度，则将小的维度扩展到大的维度
        //但是维度小的必须部分形状相似
        auto size = a.shape.size() > b.shape.size() ? b.shape.size() : a.shape.size();
        bool which = a.shape.size() > b.shape.size();
        bool flag = true;
        for (int i = 0; i < size; i++) {
            if (a.shape[a.shape.size() - 1 - i] != b.shape[b.shape.size() - 1 - i]) {
                flag = false;
                break;
            }
        }
        if (flag) {
            std::cout << "boardcasting" << std::endl;
            if (which) {
                // b的维度小
                ts::Tensor<T> tmp = b;
                //将b的维度扩展到a的维度，向前插入1
                for (int i = 0; i < a.shape.size() - b.shape.size(); i++) {
                    tmp.shape.insert(tmp.shape.begin(), 1);
                }
                tmp.init_strides();
                // tile
                std::vector<size_t> data;
                for (int i = 0; i < a.shape.size(); i++) {
                    data.push_back(a.shape[i] / tmp.shape[i]);
                }
                auto tmp2 = tile(tmp, data);
                right = tmp2;
            }
            else {
                // a的维度小
                ts::Tensor<T> tmp = a;
                //将a的维度扩展到b的维度，向前插入1
                for (int i = 0; i < b.shape.size() - a.shape.size(); i++) {
                    tmp.shape.insert(tmp.shape.begin(), 1);
                }
                tmp.init_strides();
                // tile
                std::vector<size_t> data;
                for (int i = 0; i < b.shape.size(); i++) {
                    data.push_back(b.shape[i] / tmp.shape[i]);
                }
                auto tmp2 = tile(tmp, data);
                left = tmp2;
            }
        }
        else {
            std::cout << "shape error" << std::endl;
            return ts::Tensor<T>();
        }

        //相减
        std::vector<T> data;
        for (int i = 0; i < left.originalData.size(); i++) {
            data.push_back(left.originalData[i] - right.originalData[i]);
        }
        ts::Tensor<T> history = ts::Tensor<T>(left.shape, data);
        a.dataHistory.push_back(history);
        b.dataHistory.push_back(history);
        return history;
    }

    template <typename T>
    ts::Tensor<T>
    pointwise_mul(ts::Tensor<T> & a, ts::Tensor<T> & b)
    {
        std::cout << "pointwise_mul" << std::endl;

        ts::Tensor<T> left = a;
        ts::Tensor<T> right = b;
        //从后向前判断，如果某一个维度小于另一个维度，则将小的维度扩展到大的维度
        //但是维度小的必须部分形状相似
        auto size = a.shape.size() > b.shape.size() ? b.shape.size() : a.shape.size();
        bool which = a.shape.size() > b.shape.size();
        bool flag = true;
        for (int i = 0; i < size; i++) {
            if (a.shape[a.shape.size() - 1 - i] != b.shape[b.shape.size() - 1 - i]) {
                flag = false;
                break;
            }
        }
        if (flag) {
            std::cout << "boardcasting" << std::endl;
            if (which) {
                // b的维度小
                ts::Tensor<T> tmp = b;
                //将b的维度扩展到a的维度，向前插入1
                for (int i = 0; i < a.shape.size() - b.shape.size(); i++) {
                    tmp.shape.insert(tmp.shape.begin(), 1);
                }
                tmp.init_strides();
                // tile
                std::vector<size_t> data;
                for (int i = 0; i < a.shape.size(); i++) {
                    data.push_back(a.shape[i] / tmp.shape[i]);
                }
                auto tmp2 = tile(tmp, data);
                right = tmp2;
            }
            else {
                // a的维度小
                ts::Tensor<T> tmp = a;
                //将a的维度扩展到b的维度，向前插入1
                for (int i = 0; i < b.shape.size() - a.shape.size(); i++) {
                    tmp.shape.insert(tmp.shape.begin(), 1);
                }
                tmp.init_strides();
                // tile
                std::vector<size_t> data;
                for (int i = 0; i < b.shape.size(); i++) {
                    data.push_back(b.shape[i] / tmp.shape[i]);
                }
                auto tmp2 = tile(tmp, data);
                left = tmp2;
            }
        }
        else {
            std::cout << "shape error" << std::endl;
            return ts::Tensor<T>();
        }

        //相乘
        std::vector<T> data;
        for (int i = 0; i < left.originalData.size(); i++) {
            data.push_back(left.originalData[i] * right.originalData[i]);
        }
        ts::Tensor<T> history = ts::Tensor<T>(left.shape, data);
        a.dataHistory.push_back(history);
        b.dataHistory.push_back(history);
        return history;
    }

    template <typename T>
    ts::Tensor<T>
    pointwise_div(ts::Tensor<T> & a, ts::Tensor<T> & b)
    {
        std::cout << "pointwise_div" << std::endl;

        ts::Tensor<T> left = a;
        ts::Tensor<T> right = b;
        //从后向前判断，如果某一个维度小于另一个维度，则将小的维度扩展到大的维度
        //但是维度小的必须部分形状相似
        auto size = a.shape.size() > b.shape.size() ? b.shape.size() : a.shape.size();
        bool which = a.shape.size() > b.shape.size();
        bool flag = true;
        for (int i = 0; i < size; i++) {
            if (a.shape[a.shape.size() - 1 - i] != b.shape[b.shape.size() - 1 - i]) {
                flag = false;
                break;
            }
        }
        if (flag) {
            std::cout << "boardcasting" << std::endl;
            if (which) {
                // b的维度小
                ts::Tensor<T> tmp = b;
                //将b的维度扩展到a的维度，向前插入1
                for (int i = 0; i < a.shape.size() - b.shape.size(); i++) {
                    tmp.shape.insert(tmp.shape.begin(), 1);
                }
                tmp.init_strides();
                // tile
                std::vector<size_t> data;
                for (int i = 0; i < a.shape.size(); i++) {
                    data.push_back(a.shape[i] / tmp.shape[i]);
                }
                auto tmp2 = tile(tmp, data);
                right = tmp2;
            }
            else {
                // a的维度小
                ts::Tensor<T> tmp = a;
                //将a的维度扩展到b的维度，向前插入1
                for (int i = 0; i < b.shape.size() - a.shape.size(); i++) {
                    tmp.shape.insert(tmp.shape.begin(), 1);
                }
                tmp.init_strides();
                // tile
                std::vector<size_t> data;
                for (int i = 0; i < b.shape.size(); i++) {
                    data.push_back(b.shape[i] / tmp.shape[i]);
                }
                auto tmp2 = tile(tmp, data);
                left = tmp2;
            }
        }
        else {
            std::cout << "shape error" << std::endl;
            return ts::Tensor<T>();
        }

        //相除
        std::vector<T> data;
        for (int i = 0; i < left.originalData.size(); i++) {
            data.push_back(left.originalData[i] / right.originalData[i]);
        }
        ts::Tensor<T> history = ts::Tensor<T>(left.shape, data);
        a.dataHistory.push_back(history);
        b.dataHistory.push_back(history);
        return history;
    }

    template <typename T>
    ts::Tensor<T>
    pointwise_log(ts::Tensor<T> & tensor)
    {
        std::cout << "pointwise_log" << std::endl;

        //取对数
        std::vector<T> data;
        for (int i = 0; i < tensor.originalData.size(); i++) {
            data.push_back(log(tensor.originalData[i]));
        }
        ts::Tensor<T> history = ts::Tensor<T>(tensor.shape, data);
        tensor.dataHistory.push_back(history);
        return history;
    }

    template <typename T>
    ts::Tensor<T>
    reduce_sum(const ts::Tensor<T> & tensor, size_t axis)
    {
        std::vector<T> data;
        std::vector<std::vector<T>> sliceData;
        sliceData.push_back(tensor.originalData);
        std::vector<std::vector<T>> tmpData;
        //递归到axis这个维度为止，前面的不断切片
        for (int i = 0; i <= axis; i++) {
            //对sliceData中的每一个元素进行切片
            for (int j = 0; j < sliceData.size(); j++) {
                //获取其中一个元素的长度
                int size = sliceData[0].size();
                size = size / tensor.shape[i];
                //将元素切片为size个
                for (int k = 0; k < tensor.shape[i]; k++) {
                    std::vector<T> tmp;
                    for (int l = 0; l < size; l++) {
                        tmp.push_back(sliceData[j][k * size + l]);
                    }
                    tmpData.push_back(tmp);
                }
            }
            sliceData.clear();
            sliceData = tmpData;
            tmpData.clear();
        }
        // axis之前的shape相乘
        int size = 1;
        for (int i = 0; i < axis; i++) {
            size *= tensor.shape[i];
        }
        // axis之后的shape相乘
        int size2 = 1;
        for (int i = axis + 1; i < tensor.shape.size(); i++) {
            size2 *= tensor.shape[i];
        }
        //求和
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size2; j++) {
                T sum = 0;
                for (int k = 0; k < tensor.shape[axis]; k++) {
                    sum += sliceData[i * tensor.shape[axis] + k][j];
                }
                data.push_back(sum);
            }
        }
        //删除axis这个维度
        std::vector<size_t> tmpShape = tensor.shape;
        tmpShape.erase(tmpShape.begin() + axis);
        return ts::Tensor<T>(tmpShape, data);
    }

    template <typename T>
    ts::Tensor<T>
    reduce_mean(const ts::Tensor<T> & tensor, size_t axis)
    {
        std::vector<T> data;
        std::vector<std::vector<T>> sliceData;
        sliceData.push_back(tensor.originalData);
        std::vector<std::vector<T>> tmpData;
        //递归到axis这个维度为止，前面的不断切片
        for (int i = 0; i <= axis; i++) {
            //对sliceData中的每一个元素进行切片
            for (int j = 0; j < sliceData.size(); j++) {
                //获取其中一个元素的长度
                int size = sliceData[0].size();
                size = size / tensor.shape[i];
                //将元素切片为size个
                for (int k = 0; k < tensor.shape[i]; k++) {
                    std::vector<T> tmp;
                    for (int l = 0; l < size; l++) {
                        tmp.push_back(sliceData[j][k * size + l]);
                    }
                    tmpData.push_back(tmp);
                }
            }
            sliceData.clear();
            sliceData = tmpData;
            tmpData.clear();
        }
        // axis之前的shape相乘
        int size = 1;
        for (int i = 0; i < axis; i++) {
            size *= tensor.shape[i];
        }
        // axis之后的shape相乘
        int size2 = 1;
        for (int i = axis + 1; i < tensor.shape.size(); i++) {
            size2 *= tensor.shape[i];
        }

        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size2; j++) {
                T sum = 0;
                for (int k = 0; k < tensor.shape[axis]; k++) {
                    sum += sliceData[i * tensor.shape[axis] + k][j];
                }
                data.push_back(sum / tensor.shape[axis]);
            }
        }
        //删除axis这个维度
        std::vector<size_t> tmpShape = tensor.shape;
        tmpShape.erase(tmpShape.begin() + axis);
        return ts::Tensor<T>(tmpShape, data);
    }

    template <typename T>
    ts::Tensor<T>
    reduce_max(const ts::Tensor<T> & tensor, size_t axis)
    {
        std::vector<T> data;
        std::vector<std::vector<T>> sliceData;
        sliceData.push_back(tensor.originalData);
        std::vector<std::vector<T>> tmpData;
        //递归到axis这个维度为止，前面的不断切片
        for (int i = 0; i <= axis; i++) {
            //对sliceData中的每一个元素进行切片
            for (int j = 0; j < sliceData.size(); j++) {
                //获取其中一个元素的长度
                int size = sliceData[0].size();
                size = size / tensor.shape[i];
                //将元素切片为size个
                for (int k = 0; k < tensor.shape[i]; k++) {
                    std::vector<T> tmp;
                    for (int l = 0; l < size; l++) {
                        tmp.push_back(sliceData[j][k * size + l]);
                    }
                    tmpData.push_back(tmp);
                }
            }
            sliceData.clear();
            sliceData = tmpData;
            tmpData.clear();
        }
        // axis之前的shape相乘
        int size = 1;
        for (int i = 0; i < axis; i++) {
            size *= tensor.shape[i];
        }
        // axis之后的shape相乘
        int size2 = 1;
        for (int i = axis + 1; i < tensor.shape.size(); i++) {
            size2 *= tensor.shape[i];
        }
        //
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size2; j++) {
                T max = sliceData[i * tensor.shape[axis]][j];
                for (int k = 0; k < tensor.shape[axis]; k++) {
                    if (sliceData[i * tensor.shape[axis] + k][j] > max) {
                        max = sliceData[i * tensor.shape[axis] + k][j];
                    }
                }
                data.push_back(max);
            }
        }
        //删除axis这个维度
        std::vector<size_t> tmpShape = tensor.shape;
        tmpShape.erase(tmpShape.begin() + axis);
        return ts::Tensor<T>(tmpShape, data);
    }

    template <typename T>
    ts::Tensor<T>
    reduce_min(const ts::Tensor<T> & tensor, size_t axis)
    {
        std::vector<T> data;
        std::vector<std::vector<T>> sliceData;
        sliceData.push_back(tensor.originalData);
        std::vector<std::vector<T>> tmpData;
        //递归到axis这个维度为止，前面的不断切片
        for (int i = 0; i <= axis; i++) {
            //对sliceData中的每一个元素进行切片
            for (int j = 0; j < sliceData.size(); j++) {
                //获取其中一个元素的长度
                int size = sliceData[0].size();
                size = size / tensor.shape[i];
                //将元素切片为size个
                for (int k = 0; k < tensor.shape[i]; k++) {
                    std::vector<T> tmp;
                    for (int l = 0; l < size; l++) {
                        tmp.push_back(sliceData[j][k * size + l]);
                    }
                    tmpData.push_back(tmp);
                }
            }
            sliceData.clear();
            sliceData = tmpData;
            tmpData.clear();
        }
        // axis之前的shape相乘
        int size = 1;
        for (int i = 0; i < axis; i++) {
            size *= tensor.shape[i];
        }
        // axis之后的shape相乘
        int size2 = 1;
        for (int i = axis + 1; i < tensor.shape.size(); i++) {
            size2 *= tensor.shape[i];
        }
        //
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size2; j++) {
                T min = sliceData[i * tensor.shape[axis]][j];
                for (int k = 0; k < tensor.shape[axis]; k++) {
                    if (sliceData[i * tensor.shape[axis] + k][j] < min) {
                        min = sliceData[i * tensor.shape[axis] + k][j];
                    }
                }
                data.push_back(min);
            }
        }
        //删除axis这个维度
        std::vector<size_t> tmpShape = tensor.shape;
        tmpShape.erase(tmpShape.begin() + axis);
        return ts::Tensor<T>(tmpShape, data);
    }

    // you may modify the following functions' implementation if necessary

    template <typename T>
    ts::Tensor<bool>
    eq(const ts::Tensor<T> & a, const ts::Tensor<T> & b)
    {
        std::vector<bool> data;
        int size = 1;
        for (int i = 0; i < a.shape.size(); i++) {
            size *= a.shape[i];
        }
        for (int i = 0; i < size; i++) {
            data.push_back(a.originalData[i] == b.originalData[i]);
        }
        return ts::Tensor<bool>(a.shape, data);
    }

    template <typename T>
    ts::Tensor<bool>
    ne(const ts::Tensor<T> & a, const ts::Tensor<T> & b)
    {
        std::vector<bool> data;
        int size = 1;
        for (int i = 0; i < a.shape.size(); i++) {
            size *= a.shape[i];
        }
        for (int i = 0; i < size; i++) {
            data.push_back(a.originalData[i] != b.originalData[i]);
        }
        return ts::Tensor<bool>(a.shape, data);
    }

    template <typename T>
    ts::Tensor<bool>
    gt(const ts::Tensor<T> & a, const ts::Tensor<T> & b)
    {
        std::vector<bool> data;
        int size = 1;
        for (int i = 0; i < a.shape.size(); i++) {
            size *= a.shape[i];
        }
        for (int i = 0; i < size; i++) {
            data.push_back(a.originalData[i] > b.originalData[i]);
        }
        return ts::Tensor<bool>(a.shape, data);
    }

    template <typename T>
    ts::Tensor<bool>
    ge(const ts::Tensor<T> & a, const ts::Tensor<T> & b)
    {
        std::vector<bool> data;
        int size = 1;
        for (int i = 0; i < a.shape.size(); i++) {
            size *= a.shape[i];
        }
        for (int i = 0; i < size; i++) {
            data.push_back(a.originalData[i] >= b.originalData[i]);
        }
        return ts::Tensor<bool>(a.shape, data);
    }

    template <typename T>
    ts::Tensor<bool>
    lt(const ts::Tensor<T> & a, const ts::Tensor<T> & b)
    {
        std::vector<bool> data;
        int size = 1;
        for (int i = 0; i < a.shape.size(); i++) {
            size *= a.shape[i];
        }
        for (int i = 0; i < size; i++) {
            data.push_back(a.originalData[i] < b.originalData[i]);
        }
        return ts::Tensor<bool>(a.shape, data);
    }

    template <typename T>
    ts::Tensor<bool>
    le(const ts::Tensor<T> & a, const ts::Tensor<T> & b)
    {
        std::vector<bool> data;
        int size = 1;
        for (int i = 0; i < a.shape.size(); i++) {
            size *= a.shape[i];
        }
        for (int i = 0; i < size; i++) {
            data.push_back(a.originalData[i] <= b.originalData[i]);
        }
        return ts::Tensor<bool>(a.shape, data);
    }
}

#endif // !LIBCA_UTILITY_TENSOR_HPP

// 测试代码

// int main(){
//     //生成五个随机数张量
//     auto a = bm::create_with_data({3, 3, 3}, new double[27]{1, 2, 3, 4, 5, 6, 7, 8, 9,
//                                                          1, 2, 3, 4, 5, 6, 7, 8, 9,
//                                                          1, 2, 3, 4, 5, 6, 7, 8, 9});
//     auto b = bm::create_with_data({3, 3, 3}, new double[27]{1, 2, 3, 4, 5, 6, 7, 8, 9,
//                                                          1, 2, 3, 4, 5, 6, 7, 8, 9,
//                                                          1, 2, 3, 4, 5, 6, 7, 8, 9});
//     auto c = bm::full<double>({3 , 3}, 3);
//     auto d = bm::full<double>({3 , 3}, 0);
//     auto e = bm::full<double>({3, 3, 3}, 5);
//     std::vector<ts::Tensor<double>> tensors;
//     tensors.push_back(d);
//     tensors.push_back(b);
//     tensors.push_back(c);
//     tensors.push_back(a);
//     tensors.push_back(e);

//     std::cout << "\n\n\n\n\n\t\t\t\t\tWelcome to Tensor!" << std::endl;

//     //测试PatternSpecific
//     std::cout << "\n\n\n\n\nPatternSpecific" << std::endl;
//     std::cout << "before\t" ;
//     b.show();
//     b = bm::specificPatterns(b, b.shape, 0);
//     std::cout << "after\t" ;
//     b.show();

//     std::cout << std::endl << "\n\n\n\t\t\tindex\t" << b({0, 0, 0}) << std::endl;
//     std::cout << "\t\t\tslice\t" << b({(0, 0), (0, 0), (0, 0)}) << std::endl;

//     //boardcasting test
//     std::cout << "\n\n\n\n\nBoardcasting Test" << std::endl;
//     std::cout << "before\t" ;
//     a.show();
//     c.show();
//     std::cout << "after\n" ;
//     bm::pointwise_add(a, c).show();
//     bm::pointwise_sub(a, c).show();
//     bm::pointwise_mul(a, c).show();
//     bm::pointwise_div(a, c).show();

//     std::cout << "\n\n\n\n\nFileIO" << std::endl;

//     a.writeFile("D:\\Code\\CPP\\final project guideline\\tensor\\test.txt");
//     a.readFile("D:\\Code\\CPP\\final project guideline\\tensor\\test.txt");
//     a.show();

//     auto aaaa = bm::create_with_data({3, 3, 3}, new int[27]{1, 2, 3, 4, 5, 6, 7, 8, 9,
//                                                             1, 2, 3, 4, 5, 6, 7, 8, 9,
//                                                             1, 2, 3, 4, 5, 6, 7, 8,
//                                                             9});

//     auto bbbb = bm::create_with_data({3, 3, 3}, new int[27]{1, 2, 3, 4, 5, 6, 7, 8, 9,
//                                                             1, 2, 3, 4, 5, 6, 7, 8, 9,
//                                                             1, 2, 3, 4, 5, 6, 7, 8,
//                                                             9});
//     std::vector<ts::Tensor<int>> tensors2;
//     tensors2.push_back(aaaa);
//     tensors2.push_back(bbbb);

//     //对角线元素
//     //第五个，以某个轴求和的实现
//     bm::einsum("ijk->i", tensors2).show();
//     bm::einsum("ijk->j", tensors2).show();
//     bm::einsum("ijk->k", tensors2).show();
//     //第一个，返回对角线元素的实现
//     bm::einsum("iii->i", tensors2).show();
//     //第二个，和第三个，对两个以及多个张量维度交换的实现，
//     bm::einsum("ijk->ijk", tensors2).show();
//     bm::einsum("ijk->jki", tensors2).show();
//     bm::einsum("ijk->kij", tensors2).show();
//     //第八个，第九个,第四个张量点乘总和的实现
//     bm::einsum("ijk->", tensors2).show();
//     bm::einsum("ijk,ijk->", tensors2).show();

//     //第十个向量产生矩阵的实现
//     auto cccc = bm::create_with_data({3}, new int[3]{1, 2, 3});
//     auto dddd = bm::create_with_data({3}, new int[3]{1, 2, 3});
//     auto tensors3 = std::vector<ts::Tensor<int>>();
//     tensors3.push_back(cccc);
//     tensors3.push_back(dddd);
//     bm::einsum("i,j->ij", tensors3).show();
//     //第六个矩阵乘法的实现
//     auto eeee = bm::create_with_data({3}, new int[3]{1, 2, 3});
//     auto ffff = bm::create_with_data({3, 3}, new int[9]{1, 2, 3, 4, 5, 6, 7, 8, 9});
//     auto tensors4 = std::vector<ts::Tensor<int>>();
//     tensors4.push_back(eeee);
//     tensors4.push_back(ffff);
//     bm::einsum("i,ij->j", tensors4).show();
//     auto tensors5 = std::vector<ts::Tensor<int>>();
//     tensors5.push_back(ffff);
//     tensors5.push_back(eeee);
//     bm::einsum("ji,i->j", tensors5).show();
//     //第七个矩阵乘法的实现
//     auto gggg = bm::create_with_data({3, 3}, new int[9]{1, 2, 3, 4, 5, 6, 7, 8, 9});
//     auto hhhh = bm::create_with_data({3, 3}, new int[9]{1, 2, 3, 4, 5, 6, 7, 8, 9});
//     auto tensors6 = std::vector<ts::Tensor<int>>();
//     tensors6.push_back(gggg);
//     tensors6.push_back(hhhh);
//     bm::einsum("ij,jk->ik", tensors6).show();

//     std::cout << "\n\n\n\n\ngrad" << std::endl;
//     a.show();
//     a = bm::pointwise_add(a, b);
//     a.show();
//     a = bm::pointwise_sub(a, b);
//     a.show();
//     a = bm::pointwise_mul(a, b);
//     a.show();
//     a = bm::pointwise_div(a, b);
//     a.show();
//     a = bm::pointwise_log(a);
//     a.show();
//     //自动记录计算过程，自动求导
//     auto grad = a.numericalGradient(1);
//     grad.show();
//     //返回为一个张量，可以直接使用
//     bm::pointwise_add(b, grad).show();

//     return 0;
// }