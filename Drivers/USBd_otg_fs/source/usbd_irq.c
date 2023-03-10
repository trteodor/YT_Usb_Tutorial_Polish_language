#include "usb_otg_regs.h"
#include "usbd_api.h"
#include "usbd_core.h"


static void DevResetHandler(void) {
  USB_OTG_DOEPMSK_TypeDef doepmsk;
  USB_OTG_DIEPMSK_TypeDef diepmsk;
  int i;

  doepmsk.d32 = 0;
  FS_USB_OTG_DREGS->DOEPMSK = doepmsk.d32;
  diepmsk.d32 = 0;
  FS_USB_OTG_DREGS->DIEPMSK = diepmsk.d32;
  FS_USB_OTG_DREGS->DAINTMSK = 0;
  FS_USB_OTG_DREGS->DIEPEMPMSK = 0;

  for (i = 0; i < EP_MAX_COUNT ; ++i) {
    /*rc_w1 Software can read as well as clear this bit by writing 1. Writing ‘0’ has
    no effect on the bit value.*/
    FS_USB_OTG_DINEPS[i].DIEPINTx = 0xff; 
    FS_USB_OTG_DOUTEPS[i].DOEPINTx = 0xff;
  }

  doepmsk.b.stupm = 1;
  doepmsk.b.xfrcm = 1;
  FS_USB_OTG_DREGS->DOEPMSK = doepmsk.d32;
  diepmsk.b.xfrcm = 1;
  FS_USB_OTG_DREGS->DIEPMSK = diepmsk.d32;
}

static void DevEnumerationDoneHandler(void) {
  USB_OTG_DSTS_TypeDef dsts;

  dsts.d32 = FS_USB_OTG_DREGS->DSTS;
  USBDreset(dsts.b.enumspd);
}


static void DevRxFifoNonEmptylHandler(void) {
  USB_OTG_GRXSTS_TypeDef status;

  status.d32 = FS_USB_OTG_GREGS->GRXSTSP;
  if (status.b.pktsts == GRXSTS_PKTSTS_SETUP_RECEIVED ||
      status.b.pktsts == GRXSTS_PKTSTS_OUT_RECEIVED)
    USBDdataReceived(status.b.ch_ep_num, status.b.bcnt);
}

static void DevOutEndPointTransactionHandler(void) {
  USB_OTG_DAINT_TypeDef    daint;
  USB_OTG_DOEPINTx_TypeDef doepint;
  uint32_t oepint, ep;

  daint.d32 = FS_USB_OTG_DREGS->DAINT;
  daint.d32 &= FS_USB_OTG_DREGS->DAINTMSK;
  for (oepint = daint.b.oepint, ep = 0; oepint; oepint >>= 1, ++ep) {
    if (oepint & 1) {
      doepint.d32 = FS_USB_OTG_DOUTEPS[ep].DOEPINTx;
      doepint.d32 &= FS_USB_OTG_DREGS->DOEPMSK;

      FS_USB_OTG_DOUTEPS[ep].DOEPINTx = doepint.d32;

      /* We assume that there is only one interrupt source for
         one endpoint at one time. */
      if (doepint.b.stup)
        USBDtransfer(ep, PID_SETUP);
      else if (doepint.b.xfrc)
        USBDtransfer(ep, PID_OUT);
    }
  }
}

static void DevInEndPointTransactionHandler(void) {
  USB_OTG_DAINT_TypeDef    daint;
  USB_OTG_DIEPINTx_TypeDef diepint;
  uint32_t iepint, ep, mask;

  daint.d32 = FS_USB_OTG_DREGS->DAINT;
  daint.d32 &= FS_USB_OTG_DREGS->DAINTMSK;
  for (iepint = daint.b.iepint, ep = 0; iepint; iepint >>= 1, ++ep) {
    if (iepint & 1) {
      mask = FS_USB_OTG_DREGS->DIEPMSK;
      mask |= ((FS_USB_OTG_DREGS->DIEPEMPMSK >> ep) & 1) << 7;
      diepint.d32 = FS_USB_OTG_DINEPS[ep].DIEPINTx & mask;

      /* Clear pending interrupt flags. */
      FS_USB_OTG_DINEPS[ep].DIEPINTx = diepint.d32;

      /* We assume that there is only one interrupt source for
         one endpoint at one time. */
      if(diepint.b.txfe) /* Disabled in USBDcontinueInTransfer */
        USBDcontinueInTransfer(ep);
      if (diepint.b.xfrc)
        USBDtransfer(ep, PID_IN);
    }
  }
}

static void DevSuspendHandler(void) {
  USB_OTG_GINTSTS_TypeDef gintsts;

  USBDsuspend();

  /* Clear interrupt again. */
  gintsts.d32 = 0;
  gintsts.b.usbsusp = 1;
  FS_USB_OTG_GREGS->GINTSTS = gintsts.d32;
}

static void DevWakeupHandler() {

}

static void DevSofHandler(void) {
  USB_OTG_DSTS_TypeDef    dsts;
  USB_OTG_DEPCTLx_TypeDef depctl;
  int i;

  dsts.d32 = FS_USB_OTG_DREGS->DSTS;
  for (i = 1; i < EP_MAX_COUNT; ++i) {
    depctl.d32 = FS_USB_OTG_DOUTEPS[i].DOEPCTLx;
    if (depctl.b.eptyp == ISOCHRONOUS_TRANSFER) {
      if (dsts.b.fnsof & 1)
        depctl.b.sd1pid_soddfrm = 1;
      else
        depctl.b.sd0pid_sevnfrm  = 1;
      FS_USB_OTG_DOUTEPS[i].DOEPCTLx = depctl.d32;
    }
  }

  USBDsof(dsts.b.fnsof);
}




void OTG_FS_IRQHandler(void)
{
  USB_OTG_GINTSTS_TypeDef interrupt_status;

  interrupt_status.d32 = FS_USB_OTG_GREGS->GINTSTS;
  /* Ensure that we are in the device mode. */
  if (interrupt_status.b.cmod == 0) {
    interrupt_status.d32 &= FS_USB_OTG_GREGS->GINTMSK;
    /* Clear pending interrupt flags. */
    FS_USB_OTG_GREGS->GINTSTS = interrupt_status.d32;

    if (interrupt_status.b.usbrst)
      DevResetHandler();
    if (interrupt_status.b.enumdne)
      DevEnumerationDoneHandler();

    if (interrupt_status.b.rxflvl)
      DevRxFifoNonEmptylHandler();
    if (interrupt_status.b.oepint)
      DevOutEndPointTransactionHandler();
    if (interrupt_status.b.iepint)
      DevInEndPointTransactionHandler();

    if (interrupt_status.b.usbsusp)
      DevSuspendHandler();
    if (interrupt_status.b.wkuint)
      DevWakeupHandler();

    if (interrupt_status.b.sof)
      DevSofHandler();
  }
  else {

  }
}