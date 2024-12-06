#include "Krpccontroller.h"
Krpccontroller::Krpccontroller(){
    m_failed = false;;
    m_errText = "";
}
 void Krpccontroller::Reset(){
        m_failed = false;
        m_errText = "";
 }
 bool Krpccontroller::Failed() const{
    return m_failed;
 }
 std::string Krpccontroller::ErrorText() const{
    return m_errText;
 }
 void Krpccontroller::SetFailed(const std::string &reason){
    m_failed = true;
    m_errText = reason;
 }
 //以下功能未实现rpc服务端提供的取消功能
 void Krpccontroller::StartCancel(){

 }
bool Krpccontroller::IsCanceled() const{
   return false;
}
void Krpccontroller::NotifyOnCancel(google::protobuf::Closure* callback){

}