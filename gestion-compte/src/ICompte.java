import java.rmi.Remote;
import java.rmi.RemoteException;

public interface ICompte extends Remote {
    boolean register(String username, String password) throws RemoteException;
    boolean login(String username, String password) throws RemoteException;
    boolean delete(String username, String password)throws RemoteException;
}
