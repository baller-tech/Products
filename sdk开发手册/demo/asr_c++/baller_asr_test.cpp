#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <locale.h>
#else
#include <pthread.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#endif // _WIN32

#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <memory>
#include <string.h>

#include "baller_errors.h"
#include "baller_common.h"
#include "baller_asr.h"

/**
 * ���ݿ����ֲ��baller_asr.h�е�������baller asr sdk֧�����ֳ��õ�ʹ�ó����������߸����Լ�������ȷ��������һ�ೡ����
 * 1. ��Ƶ����һ���������Ҳ���Ҫ��̬����
 * 2. ��Ƶ����һ������������Ҫ��̬����
 * 3. ��Ƶ�������������Ҳ���Ҫ��̬����
 * 4. ��Ƶ����������������Ҫ��̬����
 * 
 * ��ʾ���зֱ���ʾ�����ֳ������õĺ��Ĵ�����߼�����������Ĵ����߼��Ķ�Ӧ��ϵΪ��
 * 1. ��Ƶ����һ���������Ҳ���Ҫ��̬���� ���ص�ο� test_once_whitout_dynamic_correction ����
 * 2. ��Ƶ����һ������������Ҫ��̬���� ���ص�ο� test_once_with_dynamic_correction ����
 * 3. ��Ƶ�������������Ҳ���Ҫ��̬���� ���ص�ο� test_continue_whitout_dynamic_correction ����
 * 4. ��Ƶ����������������Ҫ��̬����  ���ص�ο� test_continue_with_dynamic_correction ����
 */

#define DATA_PATH                       ("data")
#define LANGUAGE                        ("tib_ad")
#define SAMPLE_RATE                     (8000)
#define SAMPLE_SIZE                     (16)

// pcm file
#define PCM_FILE                        ("tib_ad.pcm")

// customer information
#define ORG_ID                            (1)
#define APP_ID                            (2)
#define APP_KEY                           ("3")

#ifdef _WIN32
#define get_thread_id   ::GetCurrentThreadId()
#else  // unix-like
#define get_thread_id   pthread_self()
#endif // _WIN32 / unix-like

typedef struct t_s_thread_param {
    const char * dir;
    const char * language;
    char * pcm_data;
    int pcm_data_len;
    int loop_cnt;

    int sample_rate;
    int sample_size;
} s_thread_param;

#ifdef _WIN32
static char *
u8_to_gb(
    const char * u8_str
)
{
    int u16_count = ::MultiByteToWideChar(CP_UTF8, 0, u8_str, -1, 0, 0);
    unsigned short * u16_str = (unsigned short *)malloc(sizeof(unsigned short) * (u16_count + 1));
    memset(u16_str, 0, sizeof(unsigned short) * (u16_count + 1));
    ::MultiByteToWideChar(CP_UTF8, 0, u8_str, -1, (LPWSTR)u16_str, u16_count);

    int gb_count = ::WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)u16_str, -1, 0, 0, 0, 0);
    char * gb_str = (char *)malloc(sizeof(char) * (gb_count + 1));
    memset(gb_str, 0, sizeof(char) * (gb_count + 1));
    ::WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)u16_str, -1, gb_str, gb_count, 0, 0);

    free(u16_str);

    return gb_str;
}
#endif // _WIN32

void BallerSleepMSec(int iMSec)
{
#ifdef _WIN32
    Sleep(iMSec);
#else
    usleep(iMSec * 1000);
#endif
}

int ReadPCMData(const char* pszFile, char** ppPCMData, int* piPCMDataLen)
{
    FILE* pFile = fopen(pszFile, "rb");
    if (pFile == 0)
    {
        return 0;
    }
    fseek(pFile, 0, SEEK_END);
    *piPCMDataLen = (int)(ftell(pFile));
    if (*piPCMDataLen == 0)
    {
        fclose(pFile);
        return 0;
    }

    *ppPCMData = (char *)malloc(*piPCMDataLen);
    memset(*ppPCMData, 0, *piPCMDataLen);

    fseek(pFile, 0, SEEK_SET);
    fread(*ppPCMData, 1, *piPCMDataLen, pFile);

    fclose(pFile);
    return *piPCMDataLen;
}

void show_result(const std::vector<std::string>& vecResult)
{
    printf("result: ");
    for (std::size_t iIndex = 0; iIndex < vecResult.size(); ++iIndex)
    {
#ifdef _WIN32
        char * gb = u8_to_gb(vecResult[iIndex].c_str());
        printf("%s", gb);
        free(gb);
#else
        printf("%s", vecResult[iIndex].c_str());
#endif
    }

    printf("\n");
}

void test_once_whitout_dynamic_correction(baller_session_id session_id, char* pPCMData, int iPCMDataLen)
{
    const char* params_once = "input_mode=once";
    int iRet = BallerASRPut(session_id, params_once, pPCMData, iPCMDataLen);
    if (BALLER_SUCCESS != iRet)
    {
        printf("Call BallerASRPut failed. Return Code: %d\n", iRet);
        return;
    }

    std::vector<std::string> vecResult;
    while (1)
    {
        char *pResult = NULL;
        int iResultLen = 0;
        int iStatus = 0;

        iRet = BallerASRGet(session_id, &pResult, &iResultLen, &iStatus);
        if (BALLER_SUCCESS == iRet)
        {
            // ������ֵΪBALLER_SUCCESSʱ�����ʶ��������ʶ������״̬һ��ΪBALLER_ASR_STATUS_COMPLETE��
            if (iResultLen > 0 && pResult)
            {
                vecResult.push_back(std::string(pResult));
                show_result(vecResult);
            }
            // ʶ�����ѻ�ȡ��ϣ�����Ҫ��������BallerASRGet
            break;
        }
        else if (BALLER_MORE_RESULT == iRet)
        {
            // ��ʹ�ö�̬����ʱֻ������Ӿ�������ʶ������iStatus==BALLER_ASR_STATUS_COMPLETEʱ��ʶ����
            if (iResultLen > 0 && pResult && BALLER_ASR_STATUS_COMPLETE == iStatus)
            {
                vecResult.push_back(std::string(pResult));
                show_result(vecResult);
            }

            // ����ʶ������Ҫ��ȡ ���������BallerASRGet
            BallerSleepMSec(150);
            continue;
        }
        else
        {
            // ��ȡ������� ����Ҫ��������BallerASRGet
            printf("Call BallerASRGet failed. Return Code: %d\n", iRet);
            break;
        }
    }
} 

void test_once_with_dynamic_correction(baller_session_id session_id, char* pPCMData, int iPCMDataLen)
{
    const char* params_once = "input_mode=once";
    int iRet = BallerASRPut(session_id, params_once, pPCMData, iPCMDataLen);
    if (BALLER_SUCCESS != iRet)
    {
        printf("Call BallerASRPut failed. Return Code: %d\n", iRet);
        return;
    }

    std::vector<std::string> vecResult;
    int iLastStatus = BALLER_ASR_STATUS_COMPLETE;

    while (1)
    {
        char *pResult = NULL;
        int iResultLen = 0;
        int iStatus = 0;

        iRet = BallerASRGet(session_id, &pResult, &iResultLen, &iStatus);
        if (BALLER_SUCCESS == iRet)
        {
            if (iResultLen > 0 && pResult)
            {
                printf("[%d] %s\n", __LINE__, pResult);
                if (BALLER_ASR_STATUS_INCOMPLETE == iLastStatus)
                {
                    // �����һ�λ�ȡ�����״̬ΪBALLER_ASR_STATUS_INCOMPLETE����ʾ��һ�λ�ȡ�Ľ���ǲ������ģ����λ�ȡ�Ľ���Ƕ���һ�λ�ȡ���������
                    // ����ʹ�ñ��λ�ȡ�Ľ���滻�ϴλ�ȡ�Ľ��
                    vecResult.pop_back();
                    vecResult.push_back(std::string(pResult));
                }
                else
                {
                    // �����һ�λ�ȡ�����״̬ΪBALLER_ASR_STATUS_COMPLETE����ʾ��һ�λ�ȡ�Ľ����һ���Ӿ������Ľ�������λ�ȡ�Ľ����һ�����Ӿ�Ľ��
                    // ����ʹ�ñ��λ�ȡ�Ľ���滻�ϴλ�ȡ�Ľ��
                    vecResult.push_back(std::string(pResult));
                }
                show_result(vecResult);
            }

            // ʶ�����ѻ�ȡ��ϣ�����Ҫ��������BallerASRGet
            break;
        }
        else if (BALLER_MORE_RESULT == iRet)
        {
            // ʹ�ö�̬����ʱ��������Ӿ�������ʶ������Ҳ������Ӿ��м�״̬�Ľ��
            if (iResultLen > 0 && pResult)
            {
                if (BALLER_ASR_STATUS_INCOMPLETE == iLastStatus)
                {
                    // �����һ�λ�ȡ�����״̬ΪBALLER_ASR_STATUS_INCOMPLETE����ʾ��һ�λ�ȡ�Ľ���ǲ������ģ����λ�ȡ�Ľ���Ƕ���һ�λ�ȡ���������
                    // ����ʹ�ñ��λ�ȡ�Ľ���滻�ϴλ�ȡ�Ľ��
                    vecResult.pop_back();
                    vecResult.push_back(std::string(pResult));
                }
                else
                {
                    // �����һ�λ�ȡ�����״̬ΪBALLER_ASR_STATUS_COMPLETE����ʾ��һ�λ�ȡ�Ľ����һ���Ӿ������Ľ�������λ�ȡ�Ľ����һ�����Ӿ�Ľ��
                    // ����ʹ�ñ��λ�ȡ�Ľ���滻�ϴλ�ȡ�Ľ��
                    vecResult.push_back(std::string(pResult));
                }

                iLastStatus = iStatus;
                show_result(vecResult);
            }

            // ����ʶ������Ҫ��ȡ ���������BallerASRGet
            BallerSleepMSec(150);
            continue;
        }
        else
        {
            // ��ȡ������� ����Ҫ��������BallerASRGet
            printf("Call BallerASRGet failed. Return Code: %d\n", iRet);
            break;
        }
    }
}

void test_continue_whitout_dynamic_correction(baller_session_id session_id, char* pPCMData, int iPCMDataLen)
{
    // ģ��¼���豸�ɼ�����Ƶ���ݺ�ʵʱ�Ľ���Ƶ���ݷ��͸�sdk��ÿ����sdk����40ms��8k16bit����Ƶ����
    int iPackageSize = 16 * 40;
    int iUsedSize = 0;
    int iRet = BALLER_SUCCESS;
    std::vector<std::string> vecResult;

    char *pResult = NULL;
    int iResultLen = 0;
    int iStatus = 0;

    for (; iPCMDataLen - iUsedSize > iPackageSize; iUsedSize += iPackageSize)
    {
        const char* params_continue = "input_mode=continue";
        iRet = BallerASRPut(session_id, params_continue, pPCMData + iUsedSize, iPackageSize);
        if (BALLER_SUCCESS != iRet)
        {
            printf("Call BallerASRPut failed. Return Code: %d\n", iRet);
            return;
        }

        iRet = BallerASRGet(session_id, &pResult, &iResultLen, &iStatus);
        // continueģʽ��BallerASRPut��input_modeû�д���endʱ��BallerASRGet���᷵��BALLER_SUCCESS��
        if (BALLER_MORE_RESULT == iRet)
        {
            // ��ʹ�ö�̬����ʱֻ������Ӿ�������ʶ������iStatus==BALLER_ASR_STATUS_COMPLETEʱ��ʶ����
            if (iResultLen > 0 && pResult && BALLER_ASR_STATUS_COMPLETE == iStatus)
            {
                vecResult.push_back(std::string(pResult));
                show_result(vecResult);
            }
        }
        else
        {
            // ��ȡ������� ����Ҫ��������BallerASRGet
            printf("Call BallerASRGet failed. Return Code: %d\n", iRet);
            return;
        }
    }

    const char* params_end = "input_mode=end";
    iRet = BallerASRPut(session_id, params_end, pPCMData + iUsedSize, iPCMDataLen - iUsedSize);
    if (BALLER_SUCCESS != iRet)
    {
        printf("Call BallerASRPut failed. Return Code: %d\n", iRet);
        return;
    }

    while (1)
    {
        char *pResult = NULL;
        int iResultLen = 0;
        int iStatus = 0;

        iRet = BallerASRGet(session_id, &pResult, &iResultLen, &iStatus);
        if (BALLER_SUCCESS == iRet)
        {
            // ������ֵΪBALLER_SUCCESSʱ�����ʶ��������ʶ������״̬һ��ΪBALLER_ASR_STATUS_COMPLETE��
            if (iResultLen > 0 && pResult)
            {
                vecResult.push_back(std::string(pResult));
                show_result(vecResult);
            }

            // ʶ�����ѻ�ȡ��ϣ�����Ҫ��������BallerASRGet
            break;
        }
        else if (BALLER_MORE_RESULT == iRet)
        {
            // ��ʹ�ö�̬����ʱֻ������Ӿ�������ʶ������iStatus==BALLER_ASR_STATUS_COMPLETEʱ��ʶ����
            if (iResultLen > 0 && pResult && BALLER_ASR_STATUS_COMPLETE == iStatus)
            {
                vecResult.push_back(std::string(pResult));
                show_result(vecResult);
            }

            // ����ʶ������Ҫ��ȡ ���������BallerASRGet
            BallerSleepMSec(150);
            continue;
        }
        else
        {
            // ��ȡ������� ����Ҫ��������BallerASRGet
            printf("Call BallerASRGet failed. Return Code: %d\n", iRet);
            break;
        }
    }
}

void test_continue_with_dynamic_correction(baller_session_id session_id, char* pPCMData, int iPCMDataLen)
{
    // ģ��¼���豸�ɼ�����Ƶ���ݺ�ʵʱ�Ľ���Ƶ���ݷ��͸�sdk��ÿ����sdk����40ms��8k16bit����Ƶ����
    int iPackageSize = 16 * 40;
    int iUsedSize = 0;
    int iRet = BALLER_SUCCESS;
    int iLastStatus = BALLER_ASR_STATUS_COMPLETE;
    std::vector<std::string> vecResult;

    char *pResult = NULL;
    int iResultLen = 0;
    int iStatus = 0;

    for (; iPCMDataLen - iUsedSize > iPackageSize; iUsedSize += iPackageSize)
    {
        const char* params_continue = "input_mode=continue";
        iRet = BallerASRPut(session_id, params_continue, pPCMData + iUsedSize, iPackageSize);
        if (BALLER_SUCCESS != iRet)
        {
            printf("Call BallerASRPut failed. Return Code: %d\n", iRet);
            return;
        }

        iRet = BallerASRGet(session_id, &pResult, &iResultLen, &iStatus);
        // continueģʽ��BallerASRPut��input_modeû�д���endʱ��BallerASRGet���᷵��BALLER_SUCCESS��
        if (BALLER_MORE_RESULT == iRet)
        {
            if (iResultLen > 0 && pResult)
            {
                if (BALLER_ASR_STATUS_INCOMPLETE == iLastStatus)
                {
                    // �����һ�λ�ȡ�����״̬ΪBALLER_ASR_STATUS_INCOMPLETE����ʾ��һ�λ�ȡ�Ľ���ǲ������ģ����λ�ȡ�Ľ���Ƕ���һ�λ�ȡ���������
                    // ����ʹ�ñ��λ�ȡ�Ľ���滻�ϴλ�ȡ�Ľ��
                    vecResult.pop_back();
                    vecResult.push_back(std::string(pResult));
                }
                else
                {
                    // �����һ�λ�ȡ�����״̬ΪBALLER_ASR_STATUS_COMPLETE����ʾ��һ�λ�ȡ�Ľ����һ���Ӿ������Ľ�������λ�ȡ�Ľ����һ�����Ӿ�Ľ��
                    // ����ʹ�ñ��λ�ȡ�Ľ���滻�ϴλ�ȡ�Ľ��
                    vecResult.push_back(std::string(pResult));
                }

                iLastStatus = iStatus;
                show_result(vecResult);
            }
        }
        else
        {
            // ��ȡ������� ����Ҫ��������BallerASRGet
            printf("Call BallerASRGet failed. Return Code: %d\n", iRet);
            return;
        }
    }

    const char* params_end = "input_mode=end";
    iRet = BallerASRPut(session_id, params_end, pPCMData + iUsedSize, iPCMDataLen - iUsedSize);
    if (BALLER_SUCCESS != iRet)
    {
        printf("Call BallerASRPut failed. Return Code: %d\n", iRet);
        return;
    }

    while (1)
    {
        char *pResult = NULL;
        int iResultLen = 0;
        int iStatus = 0;

        iRet = BallerASRGet(session_id, &pResult, &iResultLen, &iStatus);
        if (BALLER_SUCCESS == iRet)
        {
            if (iResultLen > 0 && pResult)
            {

                if (BALLER_ASR_STATUS_INCOMPLETE == iLastStatus)
                {
                    // �����һ�λ�ȡ�����״̬ΪBALLER_ASR_STATUS_INCOMPLETE����ʾ��һ�λ�ȡ�Ľ���ǲ������ģ����λ�ȡ�Ľ���Ƕ���һ�λ�ȡ���������
                    // ����ʹ�ñ��λ�ȡ�Ľ���滻�ϴλ�ȡ�Ľ��
                    vecResult.pop_back();
                    vecResult.push_back(std::string(pResult));
                }
                else
                {
                    // �����һ�λ�ȡ�����״̬ΪBALLER_ASR_STATUS_COMPLETE����ʾ��һ�λ�ȡ�Ľ����һ���Ӿ������Ľ�������λ�ȡ�Ľ����һ�����Ӿ�Ľ��
                    // ����ʹ�ñ��λ�ȡ�Ľ���滻�ϴλ�ȡ�Ľ��
                    vecResult.push_back(std::string(pResult));
                }
                show_result(vecResult);
            }

            // ʶ�����ѻ�ȡ��ϣ�����Ҫ��������BallerASRGet
            break;
        }
        else if (BALLER_MORE_RESULT == iRet)
        {
            if (iResultLen > 0 && pResult)
            {

                if (BALLER_ASR_STATUS_INCOMPLETE == iLastStatus)
                {
                    // �����һ�λ�ȡ�����״̬ΪBALLER_ASR_STATUS_INCOMPLETE����ʾ��һ�λ�ȡ�Ľ���ǲ������ģ����λ�ȡ�Ľ���Ƕ���һ�λ�ȡ���������
                    // ����ʹ�ñ��λ�ȡ�Ľ���滻�ϴλ�ȡ�Ľ��
                    vecResult.pop_back();
                    vecResult.push_back(std::string(pResult));
                }
                else
                {
                    // �����һ�λ�ȡ�����״̬ΪBALLER_ASR_STATUS_COMPLETE����ʾ��һ�λ�ȡ�Ľ����һ���Ӿ������Ľ�������λ�ȡ�Ľ����һ�����Ӿ�Ľ��
                    // ����ʹ�ñ��λ�ȡ�Ľ���滻�ϴλ�ȡ�Ľ��
                    vecResult.push_back(std::string(pResult));
                }

                iLastStatus = iStatus;
                show_result(vecResult);
            }

            // ����ʶ������Ҫ��ȡ ���������BallerASRGet
            BallerSleepMSec(150);
            continue;
        }
        else
        {
            // ��ȡ������� ����Ҫ��������BallerASRGet
            printf("Call BallerASRGet failed. Return Code: %d\n", iRet);
            break;
        }
    }
} 

#ifdef _WIN32
DWORD WINAPI TestASR(LPVOID param)
#else  // unix-like
void * TestASR(void * param)
#endif // _WIN32 / unix-like
{
    int iRet = BALLER_SUCCESS;
    baller_session_id session_id = BALLER_INVALID_SESSION_ID;
    s_thread_param* thread_param = (s_thread_param *)param;

    iRet = BallerASRWorkingThread();
    if (BALLER_SUCCESS != iRet)
    {
        printf("Call BallerASRWorkingThread failed. Return Code: %d\n", iRet);
        return 0;
    }

    // Call the BallerASRSessionBegin interface to get session
    std::string session_prams = std::string("res_dir=") + std::string(thread_param->dir) + std::string(",language=") + std::string(LANGUAGE)
        + std::string(",sample_size=") + std::to_string(thread_param->sample_size) + std::string(",sample_rate=") + std::to_string(thread_param->sample_rate)
        + std::string(",engine_type=local,hardware=cpu_slow,mode=multi_row");

    printf("%s\n", session_prams.c_str());
    iRet = BallerASRSessionBegin(session_prams.c_str(), &session_id);
    if (iRet != BALLER_SUCCESS)
    {
        printf("Call BallerASRSessionBegin failed. Return Code: %d\n", iRet);
        return 0;
    }

    for (int loop_index = 0; loop_index < thread_param->loop_cnt; ++loop_index)
    {
        printf("start call test_once_whitout_dynamic_correction\n");
        test_once_whitout_dynamic_correction(session_id, thread_param->pcm_data, thread_param->pcm_data_len);

        printf("\nstart call test_once_with_dynamic_correction\n");
        test_once_with_dynamic_correction(session_id, thread_param->pcm_data, thread_param->pcm_data_len);

        printf("\nstart call test_continue_whitout_dynamic_correction\n");
        test_continue_whitout_dynamic_correction(session_id, thread_param->pcm_data, thread_param->pcm_data_len);

        printf("\nstart call test_continue_with_dynamic_correction\n");
        test_continue_with_dynamic_correction(session_id, thread_param->pcm_data, thread_param->pcm_data_len);
    }

    // Call BallerASRSessionEnd interface to release session
    iRet = BallerASRSessionEnd(session_id);
    if (iRet != BALLER_SUCCESS)
    {
        printf("Call BallerASRSessionEnd failed. Return Code: %d\n", iRet);
    }

    return 0;
}

int main()
{
    // Call BallerLogin interface to login
    std::string login_params = "org_id=" + std::to_string(ORG_ID) + ","
        + "app_id=" + std::to_string(APP_ID) + "," + "app_key=" + APP_KEY + ","
        + "license=license/baller_sdk.license,log_level=info,log_path=./baller_log/";
    int iRet = BallerLogin(login_params.c_str());
    if (iRet != BALLER_SUCCESS)
    {
        printf("Call BallerLogin failed. Return Code: %d\n", iRet);
        return 0;
    }

    // read pcm data
    char* pPCMData = 0;
    int iPCMLen = 0;
    if (0 == ReadPCMData(PCM_FILE, &pPCMData, &iPCMLen))
    {
        printf("read pcm data failed\n");

        iRet = BallerLogout();
        if (iRet != BALLER_SUCCESS)
        {
            printf("Call BallerLogout failed. Return Code: %d\n", iRet);
        }
        return 0;
    }

    s_thread_param thread_param;
    thread_param.dir = "data";
    thread_param.language = LANGUAGE;
    thread_param.loop_cnt = 1;
    thread_param.pcm_data = pPCMData;
    thread_param.pcm_data_len = iPCMLen;
    thread_param.sample_size = SAMPLE_SIZE;
    thread_param.sample_rate = SAMPLE_RATE;
    const int thread_cnt = 1;

#ifdef _WIN32
    std::vector<HANDLE> thread_handle;
    for (int thread_idx = 0; thread_idx < thread_cnt; ++thread_idx) {
        thread_handle.push_back(::CreateThread(0, 0, TestASR, &thread_param, 0, 0));
    }
    ::WaitForMultipleObjects((DWORD)thread_handle.size(), &thread_handle[0], TRUE, INFINITE);
#else  // unix-like
    std::vector<pthread_t> thread_handle;
    for (int thread_idx = 0; thread_idx < thread_cnt; ++thread_idx) {
        pthread_t sub_handle;
        pthread_create(&sub_handle, 0, TestASR, &thread_param);
        thread_handle.push_back(sub_handle);
    }
    for (int thread_idx = 0; thread_idx < thread_cnt; ++thread_idx) {
        pthread_join(thread_handle[thread_idx], 0);
    }
#endif // _WIN32 / unix-like

    free(pPCMData);
    pPCMData = 0;

    // Call BallerLogout interface to logout
    iRet = BallerLogout();
    if (iRet != BALLER_SUCCESS)
    {
        printf("Call BallerLogout failed. Return Code: %d\n", iRet);
        return 0;
    }

    return 0;
}