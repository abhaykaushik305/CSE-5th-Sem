import numpy as np
import matplotlib.pyplot as plt

def getdata(x):
    if x == 1:
        data = np.loadtxt('binclass.txt',delimiter=',')
        return data
    if x == 2 :
        data = np.loadtxt('binclassv2.txt',delimiter=',')
        return data
    
#MLE solution
def MLE(x, pos, neg):
    #calculate means using np
    meanneg = np.mean(x[neg], axis=0)
    meanpos = np.mean(x[pos], axis=0)
    

    npos = x[pos].shape[0]
    nneg = x[neg].shape[0]

    cpos = x[pos] - meanpos
    cneg = x[neg] - meanneg
    
    sigmapos = np.trace(np.matmul(cpos,cpos.T))/npos
    sigmaneg = np.trace(np.matmul(cneg,cneg.T))/nneg
    
    #resize the vector
    meanneg.reshape((1, meanneg.shape[0]))
    meanpos.reshape((1, meanpos.shape[0]))

    
    return meanpos,meanneg, sigmapos, sigmaneg 

# Plot the data points with respective color codes
def originalpoints(x, y, pos, neg):
    # neg -> blue 
    plt.plot(x[neg,0], x[neg,1], 'b*', label='Negative')
    #pos -> red
    plt.plot(x[pos,0], x[pos,1], 'r*', label='Positive')


    
# Axis boundaries
def makeaxis(x):
        
    xaxis = np.arange(np.min(x[:,0])-5,np.max(x[:,0])+5,0.05)
    yaxis = np.arange(np.min(x[:,1])-5,np.max(x[:,1])+5,0.05)
    
    return xaxis,yaxis


# Plot decision boundary

# xaxis and  yaxis contains min to max ranges for respective axis

def decisionboundary(meanpos, meanneg,sigmapos,sigmaneg, xaxis, yaxis):
    
    X, Y = np.meshgrid(xaxis, yaxis)
    
    sigma0 = sigmapos.reshape(())
    sigma1 = sigmaneg.reshape(())
    
    mean0 = meanpos.reshape((meanpos.shape[0], 1))
    mean1 = meanneg.reshape((meanneg.shape[0], 1))
    
    
    Z0 = (sigma0**(-1*mean0.shape[0]/2))
    Z0 *= np.exp((np.square(X-mean0[0]) + np.square(Y-mean0[1]))/(-2*sigma0))
    
    Z1 = (sigma1**(-1*mean1.shape[0]/2))
    Z1 *= np.exp((np.square(X-mean1[0]) + np.square(Y-mean1[1]))/(-2*sigma1))
    
    #calculated function for plt.contour
    Z = Z0 - Z1
    
    plt.contour(X, Y, Z, 0)
    
    
def main():
    
    
    part = int(input("Enter the part "))
    
    while(part!=1 and part!=2):
        part = int(input("Wrong! Enter part 1 or 2 "))
    
    conditional = input("Sigma - 'same'or 'different' ")
    
    while(conditional!='same' and conditional!='different'):
        conditional = input("Wrong! Enter 'same'or 'different' ")
    
    data = getdata(part)
    
    x = data[:,:data.shape[1]-1]
    y = data[:,data.shape[1]-1]
    
    pos = y>0
    neg = y<0
       

    meanpos,meanneg,sigmapos,sigmaneg = MLE(x, pos, neg)

    
    plt.title('Sigma '+ conditional+' and Part: ' + str(part))
    plt.xlabel('X1')
    plt.ylabel('X2')
    
    #findaxis
    
    xaxis,yaxis = makeaxis(x)

    #draw the points
    originalpoints(x, y, pos, neg)
    
    print("You requested plot for "+ conditional + " sigma case and Part "+ str(part))
    
    #draw boundary
    
    #for same both sigmas are same so pass sigmapos in both
    if conditional == 'same':
        decisionboundary(meanpos,meanneg, sigmapos,sigmapos, xaxis, yaxis)
    else :
        decisionboundary(meanpos,meanneg, sigmapos,sigmaneg, xaxis, yaxis)
        
    #plot the legend
    plt.legend(loc="upper left")
       
    plt.show()
  

main()